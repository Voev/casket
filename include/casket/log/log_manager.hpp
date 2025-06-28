#pragma once
#include <algorithm>
#include <map>
#include <memory>
#include <sstream>

#include <casket/log/logger.hpp>
#include <casket/log/console.hpp>
#include <casket/utils/singleton.hpp>
#include <casket/utils/format.hpp>

namespace casket
{

enum class Level
{
    Emergency,
    Alert,
    Critical,
    Error,
    Warning,
    Notice,
    Info,
    Debug
};

enum class Type
{
    Console
};

class LogManager final : public Singleton<LogManager>
{
public:
    LogManager();

    ~LogManager() noexcept;

    void finalize();

    void setLevel(Level level);

    Level getLevel() const;

    void enable(Type type);

    void disable(Type type);

    template <typename... Args>
    friend void emergency(nonstd::string_view str, Args&&... args);

    template <typename... Args>
    friend void alert(nonstd::string_view str, Args&&... args);

    template <typename... Args>
    friend void critical(nonstd::string_view str, Args&&... args);

    template <typename... Args>
    friend void error(nonstd::string_view str, Args&&... args);

    template <typename... Args>
    friend void warning(nonstd::string_view str, Args&&... args);

    template <typename... Args>
    friend void notice(nonstd::string_view str, Args&&... args);

    template <typename... Args>
    friend void info(nonstd::string_view str, Args&&... args);

    template <typename... Args>
    friend void debug(nonstd::string_view str, Args&&... args);

private:
    Level maxLevel_;
    std::map<Type, std::shared_ptr<Logger>> loggers_;
};

template <typename... Args> void emergency(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Emergency <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->emergency(msg); });
    }
}

template <typename... Args> void alert(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Alert <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->alert(msg); });
    }
}

template <typename... Args> void critical(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Critical <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->critical(msg); });
    }
}

template <typename... Args> void error(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Error <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->error(msg); });
    }
}

template <typename... Args> void warning(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Warning <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->warning(msg); });
    }
}

template <typename... Args> void notice(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Notice <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->notice(msg); });
    }
}

template <typename... Args> void info(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Info <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->info(msg); });
    }
}

template <typename... Args> void debug(nonstd::string_view str, Args&&... args)
{
    auto& inst = LogManager::Instance();
    if (Level::Debug <= inst.getLevel())
    {
        auto msg = format(str, std::forward<Args>(args)...);
        std::for_each(inst.loggers_.begin(), inst.loggers_.end(),
                      [&](const auto& l) { l.second->debug(msg); });
    }
}

} // namespace casket