#pragma once
#include <cstdint>
#include <casket/utils/string.hpp>

namespace casket
{

enum class LogLevel : uint8_t
{
    EMERGENCY = 0,
    ALERT = 1,
    CRITICAL = 2,
    ERROR = 3,
    WARNING = 4,
    NOTICE = 5,
    INFO = 6,
    DEBUG = 7
};

inline const char* LevelToString(LogLevel level)
{
    static const char* names[] = {"EMR", "ALR", "CRT", "ERR", "WRN", "NTC", "INF", "DBG"};
    return names[static_cast<uint8_t>(level)];
}

inline LogLevel StringToLevel(nonstd::string_view str)
{
    if (iequals(str, "alert"))
    {
        return LogLevel::ALERT;
    }
    else if (iequals(str, "crit"))
    {
        return LogLevel::CRITICAL;
    }
    else if (iequals(str, "error"))
    {
        return LogLevel::ERROR;
    }
    else if (iequals(str, "warn"))
    {
        return LogLevel::WARNING;
    }
    else if (iequals(str, "notice"))
    {
        return LogLevel::NOTICE;
    }
    else if (iequals(str, "info"))
    {
        return LogLevel::INFO;
    }
    else if (iequals(str, "debug"))
    {
        return LogLevel::DEBUG;
    }

    return LogLevel::EMERGENCY;
}


} // namespace casket