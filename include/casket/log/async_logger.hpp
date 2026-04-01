#pragma once

#include <cstdarg>
#include <cstring>
#include <chrono>
#include <cstdio>

#include <casket/log/types.hpp>
#include <casket/log/sink.hpp>
#include <casket/log/log_config.hpp>

#include <casket/lock_free/lf_object_pool.hpp>
#include <casket/lock_free/mpsc_queue.hpp>

namespace casket
{

class AsyncLogger final
{
public:
    struct Config
    {
        size_t queue_size = 16384; // размер очереди (степень двойки)
        size_t max_msg_size = 512; // максимальный размер сообщения
        size_t pool_size = 65536;  // размер пула записей
        LogLevel min_level = LogLevel::DEBUG;
        bool enable_stats = true;
    };

    struct Record
    {
        LogLevel level;
        uint16_t msg_len;
        char data[512];

        void set(LogLevel lvl, const char* msg, size_t len)
        {
            level = lvl;
            msg_len = static_cast<uint16_t>(std::min(len, sizeof(data) - 1));
            memcpy(data, msg, msg_len);
            data[msg_len] = '\0';
        }
    };

private:
    Config config_;
    ObjectPool<Record, 8192> pool_;
    MPSCQueue<Record> queue_;

    std::atomic<uint64_t> total_produced_{0};
    std::atomic<uint64_t> total_dropped_{0};

public:
    explicit AsyncLogger(const Config& config)
        : config_(config)
        , queue_(config_.queue_size)
    {
    }

    // запрещаем копирование
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    ListNode<Record>* acquireRecord() noexcept
    {
        auto* node = pool_.acquire();
        if (!node)
        {
            if (config_.enable_stats)
            {
                total_dropped_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return node;
    }

    void releaseRecord(ListNode<Record>* node) noexcept
    {
        pool_.release(node);
    }

    bool produce(ListNode<Record>* node) noexcept
    {
        if (!node)
        {
            return false;
        }

        if (config_.enable_stats)
        {
            total_produced_.fetch_add(1, std::memory_order_relaxed);
        }

        if (!queue_.push(node))
        {
            pool_.release(node);
            if (config_.enable_stats)
            {
                total_dropped_.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }

        if (config_.enable_stats)
        {
            total_produced_.fetch_add(1, std::memory_order_relaxed);
        }

        return true;
    }

    ListNode<Record>* consume() noexcept
    {
        return queue_.pop();
    }

    bool log(LogLevel level, const char* msg, size_t len) noexcept
    {
        if (level > config_.min_level)
            return false;

        if (len >= config_.max_msg_size)
        {
            len = config_.max_msg_size - 1;
        }

        auto* node = acquireRecord();
        if (!node)
        {
            return false;
        }

        node->elem.set(level, msg, len);

        return produce(node);
    }

    bool logf(LogLevel level, const char* fmt, ...)
    {
        if (level > config_.min_level)
            return false;

        auto* node = acquireRecord();
        if (!node)
        {
            return false;
        }

        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(node->elem.data, config_.max_msg_size, fmt, args);
        va_end(args);

        if (len <= 0)
        {
            releaseRecord(node);
            return false;
        }

        node->elem.level = level;
        node->elem.msg_len = len;

        return produce(node);
    }

    /// @brief Проверить, пуста ли очередь
    bool empty() const noexcept
    {
        return queue_.empty();
    }

    // ========================================================================
    // Управление
    // ========================================================================

    void setMinLevel(LogLevel level) noexcept
    {
        config_.min_level = level;
    }

    LogLevel getMinLevel() const noexcept
    {
        return config_.min_level;
    }

    struct Stats
    {
        uint64_t produced;
        uint64_t dropped;
        size_t pool_available;
        size_t pool_capacity;
        size_t queue_capacity;
    };

    Stats getStats() const noexcept
    {
        return Stats{total_produced_.load(std::memory_order_relaxed),
                     total_dropped_.load(std::memory_order_relaxed),
                     pool_.available(),
                     pool_.capacity(),
                     queue_.capacity()};
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
