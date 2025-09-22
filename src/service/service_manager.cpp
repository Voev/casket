#include <casket/service/service_manager.hpp>
#include <system_error>
#include <cstring>

namespace casket
{

void ServiceManager::setup_default_signals()
{
    int signals[] = {SIGINT, SIGTERM};
    signal_handler_.registerSignals(signals,
                                    [this](int signum)
                                    {
                                        std::cout << "Received signal " << signum << ", shutting down gracefully..."
                                                  << std::endl;
                                        this->stop();
                                    });

    signal_handler_.registerSignal(SIGHUP,
                                   [this](int signum)
                                   {
                                       std::cout << "Received SIGHUP " << signum << ", reloading configuration..."
                                                 << std::endl;
                                       // Реализация перезагрузки конфигурации
                                   });

    signal_handler_.registerSignal(SIGUSR1,
                                   [this](int signum)
                                   {
                                       std::cout << "Received SIGUSR1" << signum << ", printing statistics..."
                                                 << std::endl;
                                       this->print_statistics();
                                   });
}

void ServiceManager::setup_poll_fds(std::vector<pollfd>& fds)
{
    fds.clear();

    // Server socket
    if (server_fd_ != -1)
    {
        fds.push_back({server_fd_, POLLIN, 0});
    }

    // Signal fd
    int signal_fd = signal_handler_.getDescriptor();
    if (signal_fd != -1)
    {
        fds.push_back({signal_fd, POLLIN, 0});
    }

    // Client connections
    auto active_connections = connections_.get_active_objects();
    for (Connection* conn : active_connections)
    {
        if (conn && conn->fd != -1)
        {
            short events = POLLIN;
            if (conn->pending_requests > 0)
            {
                events |= POLLOUT;
            }
            fds.push_back({conn->fd, events, 0});
        }
    }
}

void ServiceManager::process_events(const std::vector<pollfd>& fds)
{
    for (const auto& pfd : fds)
    {
        if (pfd.revents == 0)
        {
            continue;
        }

        if (pfd.fd == server_fd_)
        {
            if (pfd.revents & POLLIN)
            {
                accept_connection();
            }
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                std::cerr << "Server socket error, stopping..." << std::endl;
                stop();
            }
        }
        else if (pfd.fd == signal_handler_.getDescriptor())
        {
            if (pfd.revents & POLLIN)
            {
                signal_handler_.processSignals();
            }
        }
        else
        {
            if (pfd.revents & POLLIN)
            {
                handle_client_input(pfd.fd);
            }
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                handle_client_error(pfd.fd);
            }
            if (pfd.revents & POLLOUT)
            {
                // Асинхронная отправка может быть реализована здесь
            }
        }
    }
}

void ServiceManager::accept_connection()
{
    if (stats_.active_connections.load(std::memory_order_acquire) >= MAX_CONNECTIONS_ * 0.95)
    {
        return;
    }

    sockaddr_un addr{};
    socklen_t len = sizeof(addr);

    int client_fd = ::accept4(server_fd_, reinterpret_cast<sockaddr*>(&addr), &len, SOCK_NONBLOCK);
    if (client_fd == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            std::cerr << "accept4 failed: " << strerror(errno) << std::endl;
        }
        return;
    }

    auto [conn_index, conn] = acquire_connection(client_fd);
    if (!conn)
    {
        ::close(client_fd);
        if (errno != EMFILE)
        {
            std::cerr << "No free connection slots available" << std::endl;
        }
    }
}

std::pair<size_t, Connection*> ServiceManager::acquire_connection(int fd)
{
    auto result = connections_.acquire_slot([fd](Connection& conn) {
        conn.initialize(fd);
    });
    
    if (result.second) {
        stats_.active_connections.fetch_add(1, std::memory_order_relaxed);
    }
    
    return result;
}

void ServiceManager::release_connection(size_t index)
{
    connections_.with_object(index, [](Connection& conn) {
        conn.close();
    });
    connections_.release_slot(index);
    stats_.active_connections.fetch_sub(1, std::memory_order_relaxed);
}

Connection* ServiceManager::find_connection(int fd)
{
    return connections_.find_object([fd](const Connection& conn) {
        return conn.fd == fd;
    });
}

void ServiceManager::handle_client_input(int fd)
{
    Connection* conn = find_connection(fd);
    if (!conn)
    {
        return;
    }

    ssize_t bytes_read;
    size_t offset = conn->read_offset;

    while ((bytes_read = ::read(conn->fd, conn->read_buffer.data() + offset, conn->read_buffer.size() - offset)) > 0)
    {
        offset += bytes_read;
        conn->read_offset = offset;
        conn->update_activity();

        process_client_buffer(conn);

        // Динамическое увеличение буфера при необходимости
        if (offset >= conn->read_buffer.size() - 256)
        {
            conn->read_buffer.resize(conn->read_buffer.size() * 2);
        }
    }

    if (bytes_read == 0)
    {
        // Connection closed by client
        handle_client_error(fd);
    }
    else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        std::cerr << "Read error on fd " << fd << ": " << strerror(errno) << std::endl;
        handle_client_error(fd);
    }
}

void ServiceManager::handle_client_error(int fd)
{
    // Ищем и закрываем соединение
    for (size_t i = 0; i < connections_.capacity(); ++i)
    {
        connections_.with_object(i, [fd](Connection& conn) {
            if (conn.fd == fd) {
                conn.close();
            }
        });
    }
}

void ServiceManager::process_client_buffer(Connection* conn)
{
    size_t offset = conn->read_offset;
    uint8_t* data = conn->read_buffer.data();

    while (offset >= sizeof(uint32_t))
    {
        uint32_t message_length;
        std::memcpy(&message_length, data, sizeof(uint32_t));

        // Проверка на максимальный размер сообщения (защита от DoS)
        if (message_length > 10 * 1024 * 1024)
        {
            std::cerr << "Message too large: " << message_length << " bytes" << std::endl;
            handle_client_error(conn->fd);
            return;
        }

        if (offset >= sizeof(uint32_t) + message_length)
        {
            // Создаем BinaryRequest безопасным способом
            BinaryRequest request_data;
            request_data.reserve(message_length);

            // Копируем данные вручную
            const uint8_t* message_start = data + sizeof(uint32_t);
            for (size_t i = 0; i < message_length; ++i)
            {
                request_data.push_back(message_start[i]);
            }

            auto [req_index, req] = requests_.acquire_slot([&](Request& req) {
                req.client_fd = conn->fd;
                req.request_data = std::move(request_data);
            });
            
            if (req)
            {
                {
                    std::lock_guard lock(queue_mutex_);
                    request_queue_.push_back(req_index);
                }

                conn->pending_requests++;
                stats_.pending_requests.fetch_add(1, std::memory_order_relaxed);
                request_cv_.notify_one();

                // Сдвигаем буфер
                size_t total_length = sizeof(uint32_t) + message_length;
                std::memmove(data, data + total_length, offset - total_length);
                offset -= total_length;
            }
        }
        else
        {
            break; // Ждем остальные данные
        }
    }

    conn->read_offset = offset;
}

void ServiceManager::worker_loop()
{
    while (running_.load(std::memory_order_acquire))
    {
        std::optional<size_t> req_index;

        {
            std::unique_lock lock(queue_mutex_);
            request_cv_.wait_for(lock, std::chrono::milliseconds(100), [this]
                                 { return !request_queue_.empty() || !running_.load(std::memory_order_acquire); });

            if (!running_.load(std::memory_order_acquire))
            {
                break;
            }

            if (request_queue_.empty())
            {
                continue;
            }

            req_index = request_queue_.front();
            request_queue_.erase(request_queue_.begin());
        }

        if (req_index.has_value())
        {
            if (req_index.value() < requests_.capacity())
            {
                Request* req = requests_.get_object(req_index.value());
                if (req)
                {
                    process_single_request(req);

                    // Отправляем ответ
                    if (req->client_fd != -1)
                    {
                        send_response(req->client_fd, req->response_data);

                        // Уменьшаем счетчик pending requests для соединения
                        Connection* conn = find_connection(req->client_fd);
                        if (conn)
                        {
                            conn->pending_requests--;
                        }
                    }

                    requests_.release_slot(req_index.value());
                }
            }
        }

        // Периодическая проверка таймаутов
        static size_t cleanup_counter = 0;
        if (++cleanup_counter % 1000 == 0)
        {
            check_request_timeouts();
        }
    }
}

void ServiceManager::timeout_check_loop()
{
    while (running_.load(std::memory_order_acquire))
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        check_connection_timeouts();
        check_request_timeouts();
    }
}

void ServiceManager::check_connection_timeouts()
{
    auto active_connections = connections_.get_active_objects();
    for (Connection* conn : active_connections)
    {
        if (conn && conn->is_timed_out())
        {
            if (conn->pending_requests == 0)
            {
                std::cout << "Closing timed out connection fd=" << conn->fd << std::endl;
                // Находим индекс и освобождаем
                for (size_t i = 0; i < connections_.capacity(); ++i)
                {
                    connections_.with_object(i, [&](Connection& c) {
                        if (&c == conn) {
                            c.close();
                            connections_.release_slot(i);
                            stats_.connection_timeouts.fetch_add(1, std::memory_order_relaxed);
                        }
                    });
                }
            }
        }
    }
}


void ServiceManager::check_request_timeouts()
{
    std::lock_guard lock(queue_mutex_);
    for (auto it = request_queue_.begin(); it != request_queue_.end();)
    {
        size_t req_index = *it;
        bool timed_out = false;
        int client_fd = -1;

        requests_.with_object(req_index, [&](Request& req) {
            timed_out = req.is_timed_out();
            client_fd = req.client_fd;
        });

        if (timed_out)
        {
            std::cerr << "Request timeout for fd=" << client_fd << std::endl;

            // Уменьшаем счетчик для соединения
            if (Connection* conn = find_connection(client_fd))
            {
                conn->pending_requests--;
            }

            // Освобождаем слот и удаляем из очереди
            requests_.release_slot(req_index);
            it = request_queue_.erase(it);
            stats_.request_timeouts.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            ++it;
        }
    }
}

void ServiceManager::process_single_request(Request* req)
{
    if (req->request_data.empty())
    {
        req->response_data = StringToBinary("ERROR: Empty request");
        return;
    }

    uint8_t command_length = req->request_data[0];
    if (req->request_data.size() < command_length + 1U)
    {
        req->response_data = StringToBinary("ERROR: Invalid request format");
        return;
    }

    std::string command(req->request_data.begin() + 1, req->request_data.begin() + 1 + command_length);
    BinaryRequest params(req->request_data.begin() + 1 + command_length, req->request_data.end());

    BinaryHandler handler;
    {
        std::lock_guard lock(handlers_mutex_);
        auto it = handlers_.find(command);
        if (it != handlers_.end())
        {
            handler = it->second;
        }
    }

    if (handler)
    {
        try
        {
            handler(params, req->response_data);
        }
        catch (const std::exception& e)
        {
            req->response_data = StringToBinary("ERROR: " + std::string(e.what()));
        }
    }
    else
    {
        req->response_data = StringToBinary("ERROR: Unknown command: " + command);
    }
}

bool ServiceManager::send_response(int client_fd, const BinaryResponse& response)
{
    Connection* conn = find_connection(client_fd);
    if (!conn || !conn->active)
    {
        return false;
    }

    // Формируем пакет: [длина ответа:4 байта][данные ответа]
    uint32_t response_length = response.size();
    std::vector<uint8_t> packet(sizeof(uint32_t) + response_length);

    std::memcpy(packet.data(), &response_length, sizeof(uint32_t));
    std::memcpy(packet.data() + sizeof(uint32_t), response.data(), response_length);

    // Отправляем данные
    const uint8_t* data = packet.data();
    size_t remaining = packet.size();

    while (remaining > 0)
    {
        ssize_t sent = ::write(conn->fd, data, remaining);
        if (sent == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::yield();
                continue;
            }
            else
            {
                handle_client_error(client_fd);
                return false;
            }
        }
        data += sent;
        remaining -= sent;
    }

    conn->last_activity = std::chrono::steady_clock::now();
    return true;
}

void ServiceManager::close_all_connections()
{
    for (size_t i = 0; i < connections_.capacity(); ++i)
    {
        connections_.with_object(i, [](Connection& conn) {
            conn.close();
        });
        connections_.release_slot(i);
    }
    stats_.active_connections.store(0, std::memory_order_release);

    // Очищаем очередь запросов
    {
        std::lock_guard lock(queue_mutex_);
        for (auto req_index : request_queue_)
        {
            requests_.release_slot(req_index);
        }
        request_queue_.clear();
    }
    stats_.pending_requests.store(0, std::memory_order_release);
}

} // namespace casket