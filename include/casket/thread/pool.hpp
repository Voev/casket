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

    template <class Func, class... Args>
    auto add(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using return_type = std::invoke_result_t<Func, Args...>;

        auto future_work = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
        auto task = std::make_shared<std::packaged_task<return_type()>>(future_work);

        auto result = task->get_future();
        addTask([task]() { (*task)(); });
        return result;
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