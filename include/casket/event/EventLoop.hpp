#pragma once
#include <queue>
#include <future>
#include <functional>
#include <mutex>
#include <condition_variable>

class EventLoop final
{
public:
    using Task = std::function<void()>;

    EventLoop() = default;
    ~EventLoop() = default;

    EventLoop(const EventLoop&) = delete;
    EventLoop(EventLoop&&) noexcept = delete;

    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop& operator=(EventLoop&&) noexcept = delete;

    template <typename Func, typename... Args>
    void add(Func&& func, Args&&... args)
    {
        using return_type = std::invoke_result_t<Func, Args...>;
        using packaged_task_type = std::packaged_task<return_type(Args && ...)>;

        packaged_task_type task(std::forward<Func>(func));

        addTask([&] { task(std::forward<Args>(args)...); });

        return task.get_future().get();
    }

    void addTask(Task task)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            eventQueue_.push(task);
        }
        cv_.notify_one();
    }

    void start()
    {
        do
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return !eventQueue_.empty(); });

                if (!eventQueue_.empty())
                {
                    task = eventQueue_.front();
                    eventQueue_.pop();
                }
            }

            if (task)
            {
                task();
            }

        } while (!stopped_);
    }

    void stop()
    {
        add([this]() { this->stopped_ = true; });
    }

private:
    std::queue<Task> eventQueue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic_bool stopped_{false};
};
