#pragma once
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <deque>

namespace casket::thread
{

class ThreadPool final
{
public:
    /// @brief Constructs a ThreadPool with a specified number of threads.
    /// @param threads The number of threads in the pool.
    ///
    /// Initializes the thread pool with a given number of worker threads.
    explicit ThreadPool(std::size_t threads)
        : stopped_(false)
    {
        for (std::size_t i = 0; i < threads; ++i)
        {
            workers_.emplace_back([this] { this->workerThread(); });
        }
    }

    /// @brief Destructor for the ThreadPool.
    ///
    /// Ensures all threads complete their current tasks and stop gracefully.
    ~ThreadPool() noexcept
    {
        stop();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) noexcept = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) noexcept = delete;

    /// @brief Adds a new task to the thread pool.
    /// @param task A function to be executed by the thread pool.
    ///
    /// The function is added to the internal task queue and will be
    /// executed by the first available worker thread.
    void add(std::function<void()> task)
    {
        tasks_.emplace_back(std::move(task));
    }

    /// @brief Submits a task for execution and returns a future for its result.
    /// @param func The function to execute.
    /// @param args Arguments to pass to the function.
    /// @return A future which will hold the result of the task.
    template <class Func, class... Args>
    auto run(Func&& func, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using return_type = std::invoke_result_t<Func, Args...>;

        auto future_work = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
        auto task = std::make_shared<std::packaged_task<return_type()>>(future_work);

        auto result = task->get_future();
        add([task]() { (*task)(); });
        return result;
    }

    void stop() noexcept
    {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopped_ == true)
            {
                return;
            }
            stopped_ = true;
            cv_.notify_all();
        }

        for (auto&& thread : workers_)
        {
            thread.join();
        }

        workers_.clear();
    }

private:
    /// @brief Main function for each worker thread.
    ///
    /// Each worker thread fetches tasks from the queue and executes them
    /// until the stop flag is set.
    void workerThread()
    {
        for (;;)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });

                if (tasks_.empty())
                {
                    if (stopped_)
                    {
                        return;
                    }
                    else
                    {
                        continue;
                    }
                }

                task = tasks_.front();
                tasks_.pop_front();
            }

            task();
        }
    }

private:
    std::vector<std::thread> workers_;
    std::condition_variable cv_;
    std::mutex mutex_;
    std::deque<std::function<void()>> tasks_;
    bool stopped_;
};

} // namespace casket::thread