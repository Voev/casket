#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <casket/log/async_logger.hpp>
#include <casket/log/log_worker.hpp>

namespace casket
{

class LogManager
{
public:
    using LoggerPtr = std::shared_ptr<AsyncLogger>;
    using Config = AsyncLogger::Config;
    
    // ========================================================================
    // Singleton access
    // ========================================================================
    
    static LogManager& Instance() noexcept
    {
        static LogManager manager;
        return manager;
    }
    
    // ========================================================================
    // Инициализация (должна быть вызвана ПЕРВОЙ в main)
    // ========================================================================
    
    void initialize(const Config& config = Config(), 
                    std::initializer_list<LogSink*> sinks = {})
    {
        std::lock_guard<std::mutex> lock(init_mutex_);
        
        if (initialized_)
            return;
        
        default_config_ = config;
        
        // Копируем синки
        std::vector<LogSink*> sinks_vec(sinks);
        
        // Создаём воркер
        worker_ = std::make_unique<LogWorker>(sinks_vec);
        
        initialized_ = true;
    }
    
    void initialize(const Config& config, const std::vector<LogSink*>& sinks)
    {
        std::lock_guard<std::mutex> lock(init_mutex_);
        
        if (initialized_)
            return;
        
        default_config_ = config;
        worker_ = std::make_unique<LogWorker>(sinks);
        initialized_ = true;
    }
    
    bool isInitialized() const noexcept
    {
        return initialized_.load(std::memory_order_acquire);
    }
    
    // ========================================================================
    // Получение логгера для текущего потока (БЕЗОПАСНАЯ ВЕРСИЯ)
    // ========================================================================
    
    LoggerPtr getLogger() noexcept
    {
        // Проверка инициализации
        if (!isInitialized())
        {
            return nullptr;
        }
        
        thread_local LoggerPtr cached_logger = nullptr;
        
        // Если уже есть кэшированный логгер
        if (cached_logger)
        {
            return cached_logger;
        }
        
        // Создаём новый логгер
        auto logger = createLogger();
        if (logger)
        {
            cached_logger = logger;
        }
        
        return logger;
    }
    
    // ========================================================================
    // Управление логгерами
    // ========================================================================
    
    void removeCurrentThreadLogger() noexcept
    {
        // Просто очищаем thread_local кэш (он пересоздастся при следующем вызове)
        // Не храним мапу логгеров, чтобы избежать мьютексов
    }
    
    // ========================================================================
    // Добавление синков
    // ========================================================================
    
    void addSink(LogSink* sink)
    {
        if (!isInitialized())
        {
            initialize();
        }
        
        if (worker_)
        {
            worker_->addSink(sink);
        }
    }
    
    void removeSink(LogSink* sink)
    {
        if (worker_)
        {
            worker_->removeSink(sink);
        }
    }
    
    // ========================================================================
    // Настройка уровня логирования
    // ========================================================================
    
    void setGlobalMinLevel(LogLevel level) noexcept
    {
        global_min_level_.store(level, std::memory_order_relaxed);
    }
    
    LogLevel getGlobalMinLevel() const noexcept
    {
        return global_min_level_.load(std::memory_order_relaxed);
    }
    
    // ========================================================================
    // Управление
    // ========================================================================
    
    void flush()
    {
        if (worker_)
        {
            worker_->flush();
        }
    }
    
    void shutdown()
    {
        if (worker_)
        {
            worker_->stop();
            worker_->wait();
        }
    }

    LogWorker* getWorker() noexcept
    {
        return worker_.get();
    }
    
private:
    LogManager() = default;
    
    ~LogManager()
    {
        shutdown();
    }
    
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    
    LoggerPtr createLogger()
    {
        if (!worker_)
        {
            return nullptr;
        }
        
        auto config = default_config_;
        config.min_level = global_min_level_.load(std::memory_order_relaxed);
        
        auto logger = std::make_shared<AsyncLogger>(config);
        
        // Регистрируем логгер в воркере
        worker_->registerLogger(logger.get());
        
        return logger;
    }
    
private:
    std::unique_ptr<LogWorker> worker_;
    Config default_config_;
    std::atomic<bool> initialized_{false};
    std::atomic<LogLevel> global_min_level_{LogLevel::DEBUG};
    std::mutex init_mutex_;
};

} // namespace casket


#define LOG_MGR() casket::LogManager::Instance()
#define LOG_GET() LOG_MGR().getLogger()

#define LOG_MGR_IMPL(level_enum, fmt, ...)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _logger = LOG_GET();                                                                                      \
        if (_logger && (level_enum) <= _logger->getMinLevel())                                                         \
        {                                                                                                              \
            _logger->logf(level_enum, fmt, ##__VA_ARGS__);                                                             \
        }                                                                                                              \
    } while (0)

#define LOG_EMERGENCY(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::EMERGENCY, fmt, ##__VA_ARGS__)
#define LOG_ALERT(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::ALERT, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::CRITICAL, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_NOTICE(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::NOTICE, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_MGR_IMPL(casket::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
