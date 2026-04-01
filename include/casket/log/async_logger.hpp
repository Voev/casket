#pragma once

#include <casket/log/types.hpp>
#include <casket/log/sink.hpp>
#include <casket/log/log_record.hpp>

#include <casket/lock_free/lf_ring_buffer.hpp>

namespace casket
{

class AsyncLogger
{
    using MessageBuffer = lf::RingBuffer<LogRecord, 4096>;

    template <size_t>
    friend class LogWorker;

    MessageBuffer buffer_;
    std::atomic<LogLevel> level_{LogLevel::WARNING};

    AsyncLogger()
    {
    }

public:
    ~AsyncLogger() noexcept
    {
    }

    static AsyncLogger& getInstance()
    {
        static AsyncLogger g_Logger;
        return g_Logger;
    }

    LogLevel getLevel() const noexcept
    {
        return level_.load(std::memory_order_relaxed);
    }

    template <typename... Args>
    bool logf(const LogLevel& level, const char* format, Args&&... args)
    {
        LogRecord record(level);
        record.format(format, std::forward<Args>(args)...);

        if (buffer_.try_push(std::move(record)))
        {
            return true;
        }

        return false;
    }

    size_t pendingMessages() const
    {
        return buffer_.size();
    }

    bool isOverloaded() const
    {
        return buffer_.full();
    }
};

} // namespace casket

#ifdef _WIN32
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

#ifdef NDEBUG
#define LOG_FORMAT_1(str) (str)
#define LOG_FORMAT_2(str, arg1) (str, arg1)
#define LOG_FORMAT_3(str, arg1, arg2) (str, arg1, arg2)
#define LOG_FORMAT_4(str, arg1, arg2, arg3) (str, arg1, arg2, arg3)
#else
#define LOG_FORMAT_1(str) (str, __FILENAME__, __LINE__, __FUNCTION__)
#define LOG_FORMAT_2(str, arg1) (str, __FILENAME__, __LINE__, __FUNCTION__, arg1)
#define LOG_FORMAT_3(str, arg1, arg2) (str, __FILENAME__, __LINE__, __FUNCTION__, arg1, arg2)
#define LOG_FORMAT_4(str, arg1, arg2, arg3) (str, __FILENAME__, __LINE__, __FUNCTION__, arg1, arg2, arg3)
#endif

#define LOG_FORMAT_N(n, ...) CONCAT(LOG_FORMAT_, n)(__VA_ARGS__)
#define LOG_FORMAT_FOR(...) LOG_FORMAT_N(VA_NARGS(__VA_ARGS__), __VA_ARGS__)

#define LOG_IMPL(level_enum, fmt, ...)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        auto& _logger = casket::AsyncLogger::getInstance();                                                            \
        if ((level_enum) >= _logger.getLevel())                                                                        \
        {                                                                                                              \
            _logger.logf(level_enum, fmt, ##__VA_ARGS__);                                                              \
        }                                                                                                              \
    } while (0)

#define LOG_EMERGENCY(fmt, ...) LOG_IMPL(casket::LogLevel::EMERGENCY, fmt, ##__VA_ARGS__)
#define LOG_ALERT(fmt, ...) LOG_IMPL(casket::LogLevel::ALERT, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) LOG_IMPL(casket::LogLevel::CRITICAL, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_IMPL(casket::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG_IMPL(casket::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_NOTICE(fmt, ...) LOG_IMPL(casket::LogLevel::NOTICE, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_IMPL(casket::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_IMPL(casket::LogLevel::DEBUG, fmt, ##__VA_ARGS__)