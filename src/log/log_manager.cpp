#include <casket/log/log_manager.hpp>

namespace casket
{

LogManager::LogManager()
    : maxLevel_{Level::Error}
{}

LogManager::~LogManager() noexcept
{
    finalize();
}

void LogManager::finalize()
{
    disable(Type::Console);
}

void LogManager::setLevel(Level level)
{
    maxLevel_ = level;
}

Level LogManager::getLevel() const
{
    return maxLevel_;
}

void LogManager::enable(Type type)
{
    switch (type)
    {
    case Type::Console:
        loggers_[type] = std::make_shared<Console>();
        break;
    }
}

void LogManager::disable(Type type)
{
    loggers_.erase(type);
}

} // namespace casket