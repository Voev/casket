#pragma once

#include <casket/nonstd/string_view.hpp>
#include <casket/signal/signal_handler.hpp>
#include <casket/service/binary_utils.hpp>

#include <casket/lock_free/lf_object_pool.hpp>
#include <casket/lock_free/queue.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <poll.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <deque>
#include <array>
#include <chrono>
#include <optional>

namespace casket
{

struct Connection
{
    int fd = -1;
    bool active = false;
    std::vector<uint8_t> readBuffer;
    size_t pendingRequests = 0;
    size_t readOffset = 0;
    std::chrono::steady_clock::time_point lastActivity;

    Connection()
        : readBuffer(8192)
    {
    }

    // Реализация виртуального метода reset из Poolable
    void reset()
    {
        // Закрываем предыдущий сокет, если был
        if (fd != -1)
        {
            ::close(fd);
            fd = -1;
        }
        active = false;
        pendingRequests = 0;
        readOffset = 0;
        lastActivity = std::chrono::steady_clock::now();
        std::fill(readBuffer.begin(), readBuffer.end(), 0);
    }

    // Дополнительный метод для инициализации с конкретным сокетом
    void initialize(int socket_fd)
    {
        fd = socket_fd;
        active = true;
        readOffset = 0;
        lastActivity = std::chrono::steady_clock::now();
        std::fill(readBuffer.begin(), readBuffer.end(), 0);

        if (fd != -1)
        {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        }
    }

    void close()
    {
        if (fd != -1)
        {
            ::close(fd);
            fd = -1;
        }
        active = false;
    }

    bool is_timed_out() const
    {
        return std::chrono::steady_clock::now() - lastActivity > std::chrono::seconds(30);
    }

    // Метод для обновления времени активности
    void update_activity()
    {
        lastActivity = std::chrono::steady_clock::now();
    }

    // Метод для добавления данных в буфер
    size_t append_to_buffer(const uint8_t* data, size_t size)
    {
        size_t bytes_to_copy = std::min(size, readBuffer.size() - readOffset);
        if (bytes_to_copy > 0)
        {
            std::memcpy(readBuffer.data() + readOffset, data, bytes_to_copy);
            readOffset += bytes_to_copy;
        }
        return bytes_to_copy;
    }

    // Метод для очистки буфера (после обработки данных)
    void clear_buffer(size_t bytes_processed)
    {
        if (bytes_processed >= readOffset)
        {
            readOffset = 0;
        }
        else
        {
            // Сдвигаем оставшиеся данные в начало буфера
            std::memmove(readBuffer.data(), readBuffer.data() + bytes_processed, readOffset - bytes_processed);
            readOffset -= bytes_processed;
        }
    }
};

struct Request
{
    int client_fd = -1;
    BinaryRequest request_data;
    BinaryResponse response_data;
    std::chrono::steady_clock::time_point created_time;

    void reset()
    {
        client_fd = -1;
        request_data.clear();
        response_data.clear();
        created_time = std::chrono::steady_clock::now();
    }

    bool is_timed_out() const
    {
        return std::chrono::steady_clock::now() - created_time > std::chrono::seconds(10);
    }
};

class ServiceManager
{
public:
    using BinaryHandler = std::function<void(const BinaryRequest&, BinaryResponse&)>;

    ServiceManager(nonstd::string_view socket_path, size_t max_connections = 10000, size_t max_requests = 100000)
        : socket_path_(socket_path)
        , MAX_CONNECTIONS_(max_connections)
        , MAX_REQUESTS_(max_requests)
        , running_(false)
        , connections_(max_connections)
        , requests_(max_requests)
    {
    }

    ~ServiceManager() noexcept
    {
        stop();
        signal_handler_.stop();
    }

    void register_handler(nonstd::string_view command, BinaryHandler handler)
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_.emplace(command, std::move(handler));
    }

    bool start()
    {
        if (running_.load(std::memory_order_acquire))
        {
            return false;
        }

        setup_default_signals();

        ::unlink(socket_path_.c_str());

        server_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (server_fd_ == -1)
        {
            return false;
        }

        int opt = 1;
        ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
        {
            ::close(server_fd_);
            return false;
        }

        if (::listen(server_fd_, 1024) == -1)
        {
            ::close(server_fd_);
            return false;
        }

        running_.store(true, std::memory_order_release);

        // Запускаем воркеры
        const size_t num_workers = std::max(1u, std::thread::hardware_concurrency());
        for (size_t i = 0; i < num_workers; ++i)
        {
            workers_.emplace_back(&ServiceManager::worker_loop, this);
        }

        // Запускаем таймер для проверки таймаутов
        timeout_thread_ = std::thread(&ServiceManager::timeout_check_loop, this);

        std::cout << "Binary server started on " << socket_path_ << " (max connections: " << MAX_CONNECTIONS_
                  << ", max requests: " << MAX_REQUESTS_ << ")" << std::endl;
        return true;
    }

    void stop() noexcept
    {
        if (!running_.exchange(false, std::memory_order_acq_rel))
        {
            return;
        }

        std::cout << "Shutting down ServiceManager..." << std::endl;

        // 1. Сначала останавливаем транспорт (если используется абстрактный транспорт)
        if (server_fd_ != -1) // Для старой версии с UNIX сокетом
        {
            ::close(server_fd_);
            server_fd_ = -1;
            ::unlink(socket_path_.c_str());
        }

        // 2. Уведомляем все ждущие потоки
        // Без condition_variable для очереди, но есть другие механизмы:
        timeout_cv_.notify_all();

        // 3. Посылаем фейковые задания в очередь, чтобы разбудить воркеров
        const size_t num_workers = workers_.size();
        for (size_t i = 0; i < num_workers * 2; ++i)
        {
            // Добавляем специальный индикатор остановки
            // Используем максимальный индекс, который заведомо невалиден
            requestQueue_.push(std::numeric_limits<size_t>::max());
        }

        // 4. Ждем завершения воркеров
        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        // 5. Ждем завершения таймерного потока
        if (timeout_thread_.joinable())
        {
            timeout_thread_.join();
        }

        // 7. Закрываем все клиентские соединения
        close_all_connections();

        // 8. Очищаем очередь запросов
        clear_request_queue();

        // 9. Останавливаем обработчик сигналов
        signal_handler_.stop();

        // 10. Дополнительная очистка для гарантии отсутствия утечек
        cleanup_resources();

        std::cout << "ServiceManager stopped successfully" << std::endl;
    }

    void run()
    {
        std::vector<pollfd> fds;

        while (running_.load(std::memory_order_acquire))
        {
            setup_poll_fds(fds);

            int timeout = 100; // 100ms
            int ready = ::poll(fds.data(), fds.size(), timeout);

            if (ready <= 0)
            {
                continue;
            }

            process_events(fds);
        }
    }

    bool is_running() const
    {
        return running_.load(std::memory_order_acquire);
    }

    struct Statistics
    {
        std::atomic<size_t> active_connections{0};
        std::atomic<size_t> pending_requests{0};
        std::atomic<size_t> total_requests_processed{0};
        std::atomic<size_t> connection_timeouts{0};
        std::atomic<size_t> request_timeouts{0};
    };

    void print_statistics() const
    {
        std::cout << "=== ServiceManager Statistics ===" << std::endl;
        std::cout << "Active connections: " << stats_.active_connections.load() << "/" << MAX_CONNECTIONS_ << std::endl;
        std::cout << "Pending requests: " << stats_.pending_requests.load() << "/" << MAX_REQUESTS_ << std::endl;
        std::cout << "Total processed: " << stats_.total_requests_processed.load() << std::endl;
        std::cout << "Connection timeouts: " << stats_.connection_timeouts.load() << std::endl;
        std::cout << "Request timeouts: " << stats_.request_timeouts.load() << std::endl;
    }

private:
    // Основные методы
    void setup_default_signals();
    void setup_poll_fds(std::vector<pollfd>& fds);
    void process_events(const std::vector<pollfd>& fds);
    void accept_connection();
    void handle_client_input(int fd);
    void handle_client_error(int fd);
    void process_client_buffer(Connection* conn);

    void worker_loop();
    void timeout_check_loop();
    void process_single_request(Request* req);
    bool send_response(int client_fd, const BinaryResponse& response);
    void close_all_connections();

    // Lock-free методы для управления пулами
    std::pair<size_t, Connection*> acquire_connection(int fd);
    void release_connection(size_t index);

    void clear_request_queue()
    {
        size_t cleared_count = 0;
        while (true)
        {
            auto req_index_opt = requestQueue_.pop();
            if (!req_index_opt)
            {
                break;
            }

            size_t index = req_index_opt.value();
            // Пропускаем специальные индикаторы остановки
            if (index < requests_.capacity())
            {
                requests_.releaseSlot(index);
                cleared_count++;
            }
        }

        stats_.pending_requests.store(0, std::memory_order_release);

        if (cleared_count > 0)
        {
            std::cout << "Cleared " << cleared_count << " pending requests from queue" << std::endl;
        }
    }

       void cleanup_resources()
    {
        // Гарантируем, что все соединения закрыты
        for (size_t i = 0; i < connections_.capacity(); ++i)
        {
            connections_.withObject(i, [](Connection& conn)
            {
                if (conn.fd != -1)
                {
                    ::close(conn.fd);
                    conn.fd = -1;
                    conn.active = false;
                }
            });
            connections_.releaseSlot(i);
        }
        
        // Очищаем статистику
        stats_.active_connections.store(0, std::memory_order_release);
        stats_.pending_requests.store(0, std::memory_order_release);
        
        // Сбрасываем флаги
        server_fd_ = -1;
    }

    // Вспомогательные методы
    Connection* find_connection(int fd);
    void check_connection_timeouts();
    void check_request_timeouts();

    const std::string socket_path_;
    const size_t MAX_CONNECTIONS_;
    const size_t MAX_REQUESTS_;
    int server_fd_ = -1;
    std::atomic<bool> running_;

    // Пул соединений
    LfObjectPool<Connection> connections_;

    // Пул запросов
    LfObjectPool<Request> requests_;

    // Очередь запросов
    lock_free::Queue<size_t> requestQueue_;

    // Обработчики сигналов
    SignalHandler signal_handler_;

    // Обработчики команд
    std::mutex handlers_mutex_;
    std::unordered_map<std::string, BinaryHandler> handlers_;

    // Потоки
    std::vector<std::thread> workers_;
    std::thread timeout_thread_;

    // Синхронизация
    std::condition_variable timeout_cv_;

    // Статистика
    Statistics stats_;
};

} // namespace casket