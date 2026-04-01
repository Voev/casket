#pragma once

#include <array>
#include <limits>
#include <syslog.h>

#include <casket/log/types.hpp>
#include <casket/log/sink.hpp>

namespace casket
{

class SyslogSink final : public LogSink
{
    static constexpr std::array<int, 8> LevelToSyslog()
    {
        return {
            LOG_EMERG,   // EMERGENCY
            LOG_ALERT,   // ALERT
            LOG_CRIT,    // CRITICAL
            LOG_ERR,     // ERROR
            LOG_WARNING, // WARNING
            LOG_NOTICE,  // NOTICE
            LOG_INFO,    // INFO
            LOG_DEBUG    // DEBUG
        };
    }

    static constexpr int ToSyslogLevel(LogLevel level) noexcept
    {
        return LevelToSyslog()[static_cast<std::underlying_type_t<LogLevel>>(level)];
    }

    static constexpr int SafeCastToInt(size_t length) noexcept
    {
        constexpr auto kMaxLength = static_cast<size_t>(std::numeric_limits<int32_t>::max());
        if (length > kMaxLength)
        {
            return kMaxLength;
        }
        return static_cast<int>(length);
    }

public:
    explicit SyslogSink(const char* ident)
    {
        openlog(ident, LOG_PID | LOG_CONS, LOG_USER);
    }

    ~SyslogSink() noexcept
    {
        closelog();
    }

    void emergency(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::EMERGENCY), "%.*s", SafeCastToInt(len), msg);
    }

    void alert(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::ALERT), "%.*s", SafeCastToInt(len), msg);
    }

    void critical(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::CRITICAL), "%.*s", SafeCastToInt(len), msg);
    }

    void error(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::ERROR), "%.*s", SafeCastToInt(len), msg);
    }

    void warning(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::WARNING), "%.*s", SafeCastToInt(len), msg);
    }

    void notice(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::NOTICE), "%.*s", SafeCastToInt(len), msg);
    }

    void info(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::INFO), "%.*s", SafeCastToInt(len), msg);
    }

    void debug(const char* msg, size_t len) override
    {
        syslog(ToSyslogLevel(LogLevel::DEBUG), "%.*s", SafeCastToInt(len), msg);
    }
};

} // namespace casket