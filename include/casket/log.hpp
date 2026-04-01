#pragma once
#include <cstring>
#include <casket/log/async_logger.hpp>

#define LOG_LEVEL(level_func, level_enum, ...)                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        if (casket::Level::level_enum <= casket::AsyncLogger::Instance().getLevel())                                   \
        {                                                                                                              \
            level_func(LOG_FORMAT_FOR(__VA_ARGS__));                                                                   \
        }                                                                                                              \
    } while (0)

#define LOG_EMERGENCY(...) LOG_LEVEL(casket::emergency, Emergency, __VA_ARGS__)
#define LOG_ALERT(...) LOG_LEVEL(casket::alert, Alert, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG_LEVEL(casket::critical, Critical, __VA_ARGS__)
#define LOG_ERROR(...) LOG_LEVEL(casket::error, Error, __VA_ARGS__)
#define LOG_WARNING(...) LOG_LEVEL(casket::warning, Warning, __VA_ARGS__)
#define LOG_NOTICE(...) LOG_LEVEL(casket::notice, Notice, __VA_ARGS__)
#define LOG_INFO(...) LOG_LEVEL(casket::info, Info, __VA_ARGS__)
#define LOG_DEBUG(...) LOG_LEVEL(casket::debug, Debug, __VA_ARGS__)