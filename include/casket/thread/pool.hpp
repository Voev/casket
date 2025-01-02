#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <casket/lock_free/queue.hpp>
#include <iostream>

namespace casket::thread
{

class ThreadPool final
{
public:
    explicit ThreadPool(std::size_t threads)
        : stop_(false)
    {
        for (std::size_t i = 0; i < threads; ++i)
        {
            workers_.emplace_back([this] { this->workerThread(); });
        }
    }

    ~ThreadPool()
    {
        enqueue([this]() { this->stop_.store(true); });

        for (auto& worker : workers_)
        {
            worker.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) noexcept = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) noexcept = delete;

    void enqueue(std::function<void()> task)
    {
        tasks_.push(std::move(task));
    }

private:
    void workerThread()
    {
        do
        {
            auto task = tasks_.pop();
            if (task.has_value())
            {
                task.value()();
            }
        } while (!stop_.load());
    }

private:
    std::vector<std::thread> workers_;
    lock_free::Queue<std::function<void()>> tasks_;
    std::atomic<bool> stop_;
};

} // namespace casket::thread