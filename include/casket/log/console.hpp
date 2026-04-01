#pragma once

#include <cstdio>
#include <cstring>


#include <casket/log/types.hpp>
#include <casket/log/sink.hpp>

namespace casket
{

struct ConsoleSinkConfig
{
    bool use_colors = true;     // использовать ANSI цвета
    bool auto_flush = false;    // автоматически сбрасывать после каждого сообщения
    bool show_timestamp = true; // показывать временные метки
    bool show_level = true;     // показывать уровень логирования
    FILE* output = stdout;      // куда писать (stdout или stderr)

    ConsoleSinkConfig() = default;
};

class ConsoleSink final : public LogSink
{
private:
    ConsoleSinkConfig config_;

    static constexpr const char* COLOR_RESET = "\033[0m";
    static constexpr const char* COLOR_RED = "\033[31m";
    static constexpr const char* COLOR_GREEN = "\033[32m";
    static constexpr const char* COLOR_YELLOW = "\033[33m";
    static constexpr const char* COLOR_BLUE = "\033[34m";
    static constexpr const char* COLOR_MAGENTA = "\033[35m";
    static constexpr const char* COLOR_CYAN = "\033[36m";
    static constexpr const char* COLOR_WHITE = "\033[37m";
    static constexpr const char* COLOR_DIM = "\033[2m";

    const char* getColor(LogLevel level) const noexcept
    {
        if (!config_.use_colors)
            return "";

        switch (level)
        {
        case LogLevel::EMERGENCY:
            return COLOR_RED;
        case LogLevel::ALERT:
            return COLOR_RED;
        case LogLevel::CRITICAL:
            return COLOR_RED;
        case LogLevel::ERROR:
            return COLOR_RED;
        case LogLevel::WARNING:
            return COLOR_YELLOW;
        case LogLevel::NOTICE:
            return COLOR_GREEN;
        case LogLevel::INFO:
            return COLOR_CYAN;
        case LogLevel::DEBUG:
            return COLOR_DIM;
        default:
            return COLOR_RESET;
        }
    }

    // форматирование timestamp (локальный буфер для производительности)
    void formatTimestamp(char* buffer, size_t size, uint64_t timestamp_us) const
    {
        time_t sec = timestamp_us / 1000000;
        int usec = timestamp_us % 1000000;

        struct tm tm_buf;
        struct tm* tm = localtime_r(&sec, &tm_buf);

        if (tm)
        {
            snprintf(buffer, size, "%02d:%02d:%02d.%06d", tm->tm_hour, tm->tm_min, tm->tm_sec, usec);
        }
        else
        {
            snprintf(buffer, size, "%llu", (unsigned long long)timestamp_us);
        }
    }

public:
    explicit ConsoleSink(const ConsoleSinkConfig& config = ConsoleSinkConfig())
        : config_(config)
    {
    }

    ConsoleSink(bool use_colors, bool auto_flush = false, FILE* output = stdout)
        : config_{use_colors, auto_flush, true, true, output}
    {
    }

    ~ConsoleSink() override = default;

    void emergency(const char* msg, size_t len) override
    {
        write(LogLevel::EMERGENCY, msg, len);
    }

    void alert(const char* msg, size_t len) override
    {
        write(LogLevel::ALERT, msg, len);
    }

    void critical(const char* msg, size_t len) override
    {
        write(LogLevel::CRITICAL, msg, len);
    }

    void error(const char* msg, size_t len) override
    {
        write(LogLevel::ERROR, msg, len);
    }

    void warning(const char* msg, size_t len) override
    {
        write(LogLevel::WARNING, msg, len);
    }

    void notice(const char* msg, size_t len) override
    {
        write(LogLevel::NOTICE, msg, len);
    }

    void info(const char* msg, size_t len) override
    {
        write(LogLevel::INFO, msg, len);
    }

    void debug(const char* msg, size_t len) override
    {
        write(LogLevel::DEBUG, msg, len);
    }

    void flush() override
    {
        fflush(config_.output);
    }

    void setUseColors(bool enable) noexcept
    {
        config_.use_colors = enable;
    }

    void setAutoFlush(bool enable) noexcept
    {
        config_.auto_flush = enable;
    }

    void setShowTimestamp(bool enable) noexcept
    {
        config_.show_timestamp = enable;
    }

    void setShowLevel(bool enable) noexcept
    {
        config_.show_level = enable;
    }

private:
    void write(LogLevel level, const char* msg, size_t len) noexcept
    {
        if (config_.show_timestamp)
        {
            auto now = std::chrono::steady_clock::now();
            uint64_t timestamp_us =
                std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

            char timebuf[32];
            formatTimestamp(timebuf, sizeof(timebuf), timestamp_us);
            fprintf(config_.output, "[%s] ", timebuf);
        }

        if (config_.show_level)
        {
            const char* color = config_.use_colors ? getColor(level) : "";
            const char* reset = config_.use_colors ? COLOR_RESET : "";
            fprintf(config_.output, "[%s%-3s%s] ", color, LevelToString(level), reset);
        }

        fwrite(msg, 1, len, config_.output);
        fprintf(config_.output, "\n");

        if (config_.auto_flush)
        {
            fflush(config_.output);
        }
    }
};

} // namespace casket