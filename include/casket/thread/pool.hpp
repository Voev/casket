#pragma once
#include <vector>
#include <thread>
#include <future>
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

    ~ThreadPool() noexcept
    {
        addTask([this]() { this->stop_.store(true); });

        for (auto& worker : workers_)
        {
            worker.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) noexcept = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) noexcept = delete;

    void addTask(std::function<void()> task)
    {
        tasks_.push(std::move(task));
    }

    template <typename Func, typename... Args>
    void add(Func&& func, Args&&... args)
    {
        using return_type = std::invoke_result_t<Func, Args...>;
        using packaged_task_type = std::packaged_task<return_type(Args && ...)>;

        packaged_task_type task(std::forward<Func>(func));

        addTask([&] { task(std::forward<Args>(args)...); });

        return task.get_future().get();
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