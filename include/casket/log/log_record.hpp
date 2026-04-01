#pragma once

#include <cstring>
#include <casket/log/types.hpp>

namespace casket
{

struct LogRecord
{
    static constexpr size_t MAX_MESSAGE_SIZE = 512;

    char data_[MAX_MESSAGE_SIZE];
    size_t size_;
    LogLevel level_;

    LogRecord()
        : size_(0)
        , level_(LogLevel::WARNING)
    {
        data_[0] = '\0';
    }

    LogRecord(LogLevel level)
        : size_(0)
        , level_(level)
    {
        data_[0] = '\0';
    }

    LogRecord(const LogRecord& other)
        : size_(other.size_)
        , level_(other.level_)
    {
        if (size_ >= MAX_MESSAGE_SIZE)
        {
            size_ = MAX_MESSAGE_SIZE - 1;
        }
        memcpy(data_, other.data_, size_);
        data_[size_] = '\0';
    }

    LogRecord(LogRecord&& other) noexcept
        : size_(other.size_)
        , level_(other.level_)
    {
        if (size_ >= MAX_MESSAGE_SIZE)
        {
            size_ = MAX_MESSAGE_SIZE - 1;
        }
        memcpy(data_, other.data_, size_);
        data_[size_] = '\0';

        other.size_ = 0;
        other.data_[0] = '\0';
    }

    LogRecord& operator=(const LogRecord& other)
    {
        if (this != &other)
        {
            level_ = other.level_;
            size_ = other.size_;

            if (size_ >= MAX_MESSAGE_SIZE)
            {
                size_ = MAX_MESSAGE_SIZE - 1;
            }

            memcpy(data_, other.data_, size_);
            data_[size_] = '\0';
        }
        return *this;
    }

    LogRecord& operator=(LogRecord&& other) noexcept
    {
        if (this != &other)
        {
            level_ = other.level_;
            size_ = other.size_;

            if (size_ >= MAX_MESSAGE_SIZE)
            {
                size_ = MAX_MESSAGE_SIZE - 1;
            }

            memcpy(data_, other.data_, size_);
            data_[size_] = '\0';

            other.size_ = 0;
            other.data_[0] = '\0';
        }
        return *this;
    }

    template <typename... Args>
    void format(const char* format, Args&&... args)
    {
        int written = snprintf(data_, MAX_MESSAGE_SIZE, format, std::forward<Args>(args)...);

        if (written < 0)
        {
            size_ = 0;
            data_[0] = '\0';
        }
        else if (static_cast<size_t>(written) >= MAX_MESSAGE_SIZE)
        {
            size_ = MAX_MESSAGE_SIZE - 1;
            data_[size_] = '\0';
        }
        else
        {
            size_ = static_cast<size_t>(written);
        }
    }

    template <typename... Args>
    void format(LogLevel level, const char* format, Args&&... args)
    {
        level_ = level;
        format(format, std::forward<Args>(args)...);
    }

    bool empty() const
    {
        return size_ == 0;
    }

    size_t size() const
    {
        return size_;
    }

    const char* data() const
    {
        return data_;
    }

    LogLevel level() const
    {
        return level_;
    }

    void clear()
    {
        size_ = 0;
        data_[0] = '\0';
    }
};

} // namespace casket