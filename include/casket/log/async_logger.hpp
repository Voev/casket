#pragma once

#include <casket/log/types.hpp>
#include <casket/log/sink.hpp>
#include <casket/log/log_record.hpp>
#include <casket/lock_free/lf_ring_buffer.hpp>

#include <atomic>
#include <iostream>

namespace casket
{

class AsyncLogger
{
    using MessageBuffer = lf::RingBuffer<LogRecord, 4096>;

    template <size_t>
    friend class LogWorker;

    MessageBuffer buffer_;
    std::atomic<LogLevel> level_{LogLevel::WARNING};

    struct Stats
    {
        std::atomic<size_t> pushed{0};
        std::atomic<size_t> dropped{0};
    } stats_;

    AsyncLogger() = default;

public:
    ~AsyncLogger() noexcept = default;

    static AsyncLogger& getInstance()
    {
        static AsyncLogger g_Logger;
        return g_Logger;
    }

    LogLevel getLevel() const noexcept
    {
        return level_.load(std::memory_order_relaxed);
    }

    void setLevel(LogLevel level) noexcept
    {
        level_.store(level, std::memory_order_relaxed);
    }

    template <typename... Args>
    bool logf(const LogLevel& level, const char* format, Args&&... args)
    {
        if (level < getLevel())
        {
            return false;
        }

        LogRecord record(level);
        record.format(format, std::forward<Args>(args)...);

        if (buffer_.try_push(std::move(record)))
        {
            stats_.pushed.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        stats_.dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    size_t pushed() const
    {
        return stats_.pushed.load(std::memory_order_relaxed);
    }

    size_t dropped() const
    {
        return stats_.dropped.load(std::memory_order_relaxed);
    }

    size_t pending() const
    {
        return buffer_.size();
    }

    bool isOverloaded() const
    {
        return buffer_.full();
    }

    void printStats(std::ostream& os = std::cout) const
    {
        size_t p = pushed();
        size_t d = dropped();
        size_t pend = pending();
        size_t total = p + d;

        os << "=== AsyncLogger Stats ===\n"
           << "Pushed:   " << p << "\n"
           << "Dropped:  " << d << "\n"
           << "Pending:  " << pend << "\n"
           << "Total:    " << total << "\n"
           << "Drop rate: " << std::fixed << (total > 0 ? (100.0 * d / total) : 0.0) << "%\n"
           << "Overloaded: " << (isOverloaded() ? "YES" : "no") << "\n";
    }

    friend std::ostream& operator<<(std::ostream& os, const AsyncLogger& logger)
    {
        logger.printStats(os);
        return os;
    }

    void resetStats()
    {
        stats_.pushed.store(0, std::memory_order_relaxed);
        stats_.dropped.store(0, std::memory_order_relaxed);
    }
};

} // namespace casket

namespace casket::log_detail
{
inline const char* getFileName(const char* path)
{
#ifdef _WIN32
    const char* sep = strrchr(path, '\\');
#else
    const char* sep = strrchr(path, '/');
#endif
    return sep ? sep + 1 : path;
}
} // namespace casket::log_detail

#define LOG_IMPL(level_enum, fmt, ...)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        auto& _logger = casket::AsyncLogger::getInstance();                                                            \
        if ((level_enum) <= _logger.getLevel())                                                                        \
        {                                                                                                              \
            _logger.logf(level_enum, fmt, ##__VA_ARGS__);                                                              \
        }                                                                                                              \
    } while (0)

#ifdef NDEBUG
#define LOG_EMERGENCY(fmt, ...) LOG_IMPL(casket::LogLevel::EMERGENCY, fmt, ##__VA_ARGS__)
#define LOG_ALERT(fmt, ...) LOG_IMPL(casket::LogLevel::ALERT, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) LOG_IMPL(casket::LogLevel::CRITICAL, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_IMPL(casket::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG_IMPL(casket::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_NOTICE(fmt, ...) LOG_IMPL(casket::LogLevel::NOTICE, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_IMPL(casket::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_IMPL(casket::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOG_EMERGENCY(fmt, ...)                                                                                        \
    LOG_IMPL(casket::LogLevel::EMERGENCY "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__,          \
             ##__VA_ARGS__)
#define LOG_ALERT(fmt, ...)                                                                                            \
    LOG_IMPL(casket::LogLevel::ALERT, "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__,             \
             ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...)                                                                                         \
    LOG_IMPL(casket::LogLevel::CRITICAL, "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__,          \
             ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)                                                                                            \
    LOG_IMPL(casket::LogLevel::ERROR, "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__,             \
             ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)                                                                                          \
    LOG_IMPL(casket::LogLevel::WARNING, "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__,           \
             ##__VA_ARGS__)
#define LOG_NOTICE(fmt, ...)                                                                                           \
    LOG_IMPL(casket::LogLevel::NOTICE, "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__,            \
             ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                                                             \
    LOG_IMPL(casket::LogLevel::INFO, "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)                                                                                            \
    LOG_IMPL(casket::LogLevel::DEBUG, "[%s:%d] " fmt, casket::log_detail::getFileName(__FILE__), __LINE__,             \
             ##__VA_ARGS__)
#endif