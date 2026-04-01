#pragma once
#include <thread>
#include <casket/log/async_logger.hpp>

namespace casket
{

template <size_t BatchSize = 8192>
class LogWorker final
{
private:
    std::vector<LogSink*> sinks_;
    std::thread workerThread_;
    std::atomic<bool> running_{true};

    LogRecord batch_[BatchSize];

    struct alignas(64) Stats
    {
        size_t batchCount{0};
        size_t batchSizeSum{0};
        size_t maxBatchSize{0};
    } stats_;

public:
    LogWorker(std::initializer_list<LogSink*> sinks)
        : sinks_(sinks)
    {
        workerThread_ = std::thread([this]() { run(); });
    }

    ~LogWorker() noexcept
    {
        if (workerThread_.joinable())
        {
            workerThread_.join();
        }

        for (auto* sink : sinks_)
        {
            delete sink;
        }
    }

    void stop() noexcept
    {
        running_ = false;
    }

private:
    void run()
    {
        auto& queue = AsyncLogger::getInstance().buffer_;
        while (running_)
        {
            size_t popped = queue.try_pop_batch(batch_, BatchSize);

            if (popped > 0)
            {
                stats_.batchCount++;
                stats_.batchSizeSum += popped;

                size_t currentMax = stats_.maxBatchSize;
                if (popped > currentMax)
                {
                    currentMax = popped;
                }

                for (size_t i = 0; i < popped; ++i)
                {
                    spill(batch_[i]);
                }
                continue;
            }

            static int sleepUs = 100;
            if (queue.empty())
            {
                std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
                sleepUs = std::min(sleepUs * 2, 10000);
            }
            else
            {
                sleepUs = 100;
                std::this_thread::yield();
            }
        }

        size_t remaining;
        do
        {
            remaining = queue.try_pop_batch(batch_, BatchSize);
            for (size_t i = 0; i < remaining; ++i)
            {
                spill(batch_[i]);
            }
        } while (remaining > 0);
    }

    inline void spill(const LogRecord& record)
    {
        for (auto* sink : sinks_)
        {
            dispatchToSink(sink, record);
        }
    }

    void dispatchToSink(LogSink* sink, const LogRecord& record)
    {
        switch (record.level())
        {
        case LogLevel::EMERGENCY:
            sink->emergency(record.data(), record.size());
            break;
        case LogLevel::ALERT:
            sink->alert(record.data(), record.size());
            break;
        case LogLevel::CRITICAL:
            sink->critical(record.data(), record.size());
            break;
        case LogLevel::ERROR:
            sink->error(record.data(), record.size());
            break;
        case LogLevel::WARNING:
            sink->warning(record.data(), record.size());
            break;
        case LogLevel::NOTICE:
            sink->notice(record.data(), record.size());
            break;
        case LogLevel::INFO:
            sink->info(record.data(), record.size());
            break;
        case LogLevel::DEBUG:
            sink->debug(record.data(), record.size());
            break;
        }
    }
};

} // namespace casket