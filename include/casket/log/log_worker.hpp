#pragma once
#include <thread>
#include <casket/log/async_logger.hpp>

namespace casket
{

class LogWorker
{
private:
    AsyncLogger& logger_;
    std::vector<LogSink*> sinks_;
    std::thread worker_thread_;
    std::atomic<bool> running_{true};

public:
    LogWorker(AsyncLogger& logger, std::initializer_list<LogSink*> sinks)
        : logger_(logger)
        , sinks_(sinks)
    {
        worker_thread_ = std::thread([this]() { run(); });
    }

    ~LogWorker()
    {
        running_ = false;
        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }
        for (auto* sink : sinks_)
        {
            delete sink;
        }
    }

    void run()
    {
        ListNode<AsyncLogger::Record>* node = nullptr;

        while (running_ || !logger_.empty())
        {
            node = logger_.consume();
            if (node)
            {
                for (auto* sink : sinks_)
                {
                    dispatchToSink(sink, node->elem);
                }
                logger_.releaseRecord(node);
            }

            else
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

private:
    void dispatchToSink(LogSink* sink, const AsyncLogger::Record& record)
    {
        switch (record.level)
        {
        case LogLevel::EMERGENCY:
            sink->emergency(record.data, record.msg_len);
            break;
        case LogLevel::ALERT:
            sink->alert(record.data, record.msg_len);
            break;
        case LogLevel::CRITICAL:
            sink->critical(record.data, record.msg_len);
            break;
        case LogLevel::ERROR:
            sink->error(record.data, record.msg_len);
            break;
        case LogLevel::WARNING:
            sink->warning(record.data, record.msg_len);
            break;
        case LogLevel::NOTICE:
            sink->notice(record.data, record.msg_len);
            break;
        case LogLevel::INFO:
            sink->info(record.data, record.msg_len);
            break;
        case LogLevel::DEBUG:
            sink->debug(record.data, record.msg_len);
            break;
        }
    }
};

} // namespace casket