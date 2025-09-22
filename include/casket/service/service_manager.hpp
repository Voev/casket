#pragma once

#include <casket/nonstd/string_view.hpp>
#include <casket/signal/signal_handler.hpp>
#include <casket/service/binary_utils.hpp>
#include <casket/lock_free/lf_object_pool.hpp>
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
    std::vector<uint8_t> read_buffer;
    size_t pending_requests = 0;
    size_t read_offset = 0;
    std::chrono::steady_clock::time_point last_activity;

    Connection()
        : read_buffer(8192)
    {
    }

    // Реализация виртуального метода reset из Poolable
    void reset()
    {
        // Закрываем предыдущий сокет, если был
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
        active = false;
        pending_requests = 0;
        read_offset = 0;
        last_activity = std::chrono::steady_clock::now();
        std::fill(read_buffer.begin(), read_buffer.end(), 0);
    }

    // Дополнительный метод для инициализации с конкретным сокетом
    void initialize(int socket_fd)
    {
        fd = socket_fd;
        active = true;
        read_offset = 0;
        last_activity = std::chrono::steady_clock::now();
        std::fill(read_buffer.begin(), read_buffer.end(), 0);
        
        if (fd != -1) {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        }
    }

    void close()
    {
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
        active = false;
    }

    bool is_timed_out() const
    {
        return std::chrono::steady_clock::now() - last_activity > std::chrono::seconds(30);
    }

    // Метод для обновления времени активности
    void update_activity()
    {
        last_activity = std::chrono::steady_clock::now();
    }

    // Метод для добавления данных в буфер
    size_t append_to_buffer(const uint8_t* data, size_t size)
    {
        size_t bytes_to_copy = std::min(size, read_buffer.size() - read_offset);
        if (bytes_to_copy > 0) {
            std::memcpy(read_buffer.data() + read_offset, data, bytes_to_copy);
            read_offset += bytes_to_copy;
        }
        return bytes_to_copy;
    }

    // Метод для очистки буфера (после обработки данных)
    void clear_buffer(size_t bytes_processed)
    {
        if (bytes_processed >= read_offset) {
            read_offset = 0;
        } else {
            // Сдвигаем оставшиеся данные в начало буфера
            std::memmove(read_buffer.data(), 
                        read_buffer.data() + bytes_processed, 
                        read_offset - bytes_processed);
            read_offset -= bytes_processed;
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

        // Оповещаем все ждущие потоки
        request_cv_.notify_all();
        timeout_cv_.notify_all();

        // Ждем завершения воркеров
        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        // Ждем завершения таймерного потока
        if (timeout_thread_.joinable())
        {
            timeout_thread_.join();
        }

        // Закрываем серверный сокет
        if (server_fd_ != -1)
        {
            ::close(server_fd_);
            server_fd_ = -1;
        }

        // Закрываем все клиентские соединения
        close_all_connections();

        ::unlink(socket_path_.c_str());

        std::cout << "ServiceManager stopped" << std::endl;
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
    std::mutex queue_mutex_;
    std::condition_variable request_cv_;
    std::vector<size_t> request_queue_;

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