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

    signal_handler_.registerSignal(
        SIGHUP, [this](int signum)
        { std::cout << "Received SIGHUP " << signum << ", reloading configuration..." << std::endl; });

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
    auto active_connections = connections_.getActiveObjects();
    for (Connection* conn : active_connections)
    {
        if (conn && conn->fd != -1)
        {
            short events = POLLIN;
            if (conn->pendingRequests > 0)
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
    auto result = connections_.acquireSlot([fd](Connection& conn) { conn.initialize(fd); });

    if (result.second)
    {
        stats_.active_connections.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

void ServiceManager::release_connection(size_t index)
{
    connections_.withObject(index, [](Connection& conn) { conn.close(); });
    connections_.releaseSlot(index);
    stats_.active_connections.fetch_sub(1, std::memory_order_relaxed);
}

Connection* ServiceManager::find_connection(int fd)
{
    return connections_.findObject([fd](const Connection& conn) { return conn.fd == fd; });
}

void ServiceManager::handle_client_input(int fd)
{
    Connection* conn = find_connection(fd);
    if (!conn)
    {
        return;
    }

    ssize_t bytes_read;
    size_t offset = conn->readOffset;

    while ((bytes_read = ::read(conn->fd, conn->readBuffer.data() + offset, conn->readBuffer.size() - offset)) > 0)
    {
        offset += bytes_read;
        conn->readOffset = offset;
        conn->update_activity();

        process_client_buffer(conn);

        // Динамическое увеличение буфера при необходимости
        if (offset >= conn->readBuffer.size() - 256)
        {
            conn->readBuffer.resize(conn->readBuffer.size() * 2);
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
        connections_.withObject(i,
                                [fd](Connection& conn)
                                {
                                    if (conn.fd == fd)
                                    {
                                        conn.close();
                                    }
                                });
    }
}

void ServiceManager::process_client_buffer(Connection* conn)
{
    size_t offset = conn->readOffset;
    uint8_t* data = conn->readBuffer.data();

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

            auto [req_index, req] = requests_.acquireSlot(
                [&](Request& req)
                {
                    req.client_fd = conn->fd;
                    req.request_data = std::move(request_data);
                });

            if (req)
            {
                requestQueue_.push(req_index);

                conn->pendingRequests++;
                stats_.pending_requests.fetch_add(1, std::memory_order_relaxed);

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

    conn->readOffset = offset;
}

void ServiceManager::worker_loop()
{
    while (running_.load(std::memory_order_acquire))
    {
        // БЕЗ МЬЮТЕКСА - lock-free операция
        auto req_index_opt = requestQueue_.pop();
        
        if (!req_index_opt)
        {
            // Если очередь пуста и сервер еще работает
            if (running_.load(std::memory_order_acquire))
            {
                // Умная пауза: меньше паузы при высокой нагрузке
                static thread_local size_t empty_cycles = 0;
                if (empty_cycles++ < 100)
                {
                    std::this_thread::yield();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    empty_cycles = 0;
                }
            }
            continue;
        }

        size_t req_index = req_index_opt.value();

        if (req_index < requests_.capacity())
        {
            Request* req = requests_.getObject(req_index);
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
                        conn->pendingRequests--;
                    }
                }

                requests_.releaseSlot(req_index);
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
    auto active_connections = connections_.getActiveObjects();
    for (Connection* conn : active_connections)
    {
        if (conn && conn->is_timed_out())
        {
            if (conn->pendingRequests == 0)
            {
                std::cout << "Closing timed out connection fd=" << conn->fd << std::endl;
                // Находим индекс и освобождаем
                for (size_t i = 0; i < connections_.capacity(); ++i)
                {
                    connections_.withObject(i,
                                            [&](Connection& c)
                                            {
                                                if (&c == conn)
                                                {
                                                    c.close();
                                                    connections_.releaseSlot(i);
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
    // Внимание: С lock-free очередью мы не можем просто итерировать по элементам
    // Одна из стратегий:
    // 1. Создаем временную очередь для не-таймаутных запросов
    // 2. Или используем отдельную структуру для отслеживания таймаутов
    
    // Вариант 1: Используем временную очередь
    lock_free::Queue<size_t> temp_queue;
    size_t timed_out_count = 0;
    
    while (true)
    {
        auto req_index_opt = requestQueue_.pop();
        if (!req_index_opt)
        {
            break;
        }
        
        size_t req_index = req_index_opt.value();
        bool timed_out = false;
        int client_fd = -1;
        
        requests_.withObject(req_index,
                             [&](Request& req)
                             {
                                 timed_out = req.is_timed_out();
                                 client_fd = req.client_fd;
                             });
        
        if (timed_out)
        {
            std::cerr << "Request timeout for fd=" << client_fd << std::endl;

            if (Connection* conn = find_connection(client_fd))
            {
                conn->pendingRequests--;
            }

            requests_.releaseSlot(req_index);
            stats_.request_timeouts.fetch_add(1, std::memory_order_relaxed);
            timed_out_count++;
        }
        else
        {
            // Возвращаем обратно в новую очередь
            temp_queue.push(req_index);
        }
    }
    
    // Переносим все обратно в основную очередь
    while (true)
    {
        auto req_index_opt = temp_queue.pop();
        if (!req_index_opt)
        {
            break;
        }
        requestQueue_.push(req_index_opt.value());
    }
    
    if (timed_out_count > 0)
    {
        std::cout << "Removed " << timed_out_count << " timed out requests" << std::endl;
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

    conn->lastActivity = std::chrono::steady_clock::now();
    return true;
}

void ServiceManager::close_all_connections()
{
    for (size_t i = 0; i < connections_.capacity(); ++i)
    {
        connections_.withObject(i, [](Connection& conn) { conn.close(); });
        connections_.releaseSlot(i);
    }
    stats_.active_connections.store(0, std::memory_order_release);

    // Очищаем очередь запросов (lock-free)
    while (true)
    {
        auto req_index_opt = requestQueue_.pop();
        if (!req_index_opt)
        {
            break;
        }
        requests_.releaseSlot(req_index_opt.value());
    }
    stats_.pending_requests.store(0, std::memory_order_release);
}

} // namespace casket