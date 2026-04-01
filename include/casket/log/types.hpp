#pragma once
#include <cstdint>

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

} // namespace casket