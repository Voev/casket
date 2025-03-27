#include <gtest/gtest.h>
#include <chrono>
#include <atomic>
#include <casket/thread/pool.hpp>

using namespace casket::thread;
using namespace std::chrono_literals;

TEST(ThreadPoolTest, BasicFunctionality)
{
    ThreadPool pool(4);
    std::atomic<int> counter = 0;

    auto task = [&counter]()
    {
        std::this_thread::sleep_for(50ms);
        ++counter;
    };

    for (int i = 0; i < 10; ++i)
    {
        pool.add(task);
    }

    std::this_thread::sleep_for(1s);

    ASSERT_EQ(counter.load(), 10);
}

TEST(ThreadPoolTest, AddTask)
{
    ThreadPool pool(2);
    std::atomic<int> result = 0;

    pool.add(
        [&result]()
        {
            std::this_thread::sleep_for(100ms);
            result.fetch_add(1);
        });

    pool.add(
        [&result]()
        {
            std::this_thread::sleep_for(150ms);
            result.fetch_add(2);
        });

    std::this_thread::sleep_for(300ms);

    ASSERT_EQ(result.load(), 3);
}

TEST(ThreadPoolTest, Add)
{
    ThreadPool pool(2);
    std::atomic<int> result = 0;

    pool.add(
        [&result]()
        {
            std::this_thread::sleep_for(100ms);
            result.fetch_add(1);
        });

    pool.add(
        [&result]()
        {
            std::this_thread::sleep_for(150ms);
            result.fetch_add(2);
        });

    std::this_thread::sleep_for(300ms);

    ASSERT_EQ(result.load(), 3);
}

TEST(ThreadPoolTest, DestructorWaitsForTasks)
{
    std::atomic<bool> taskFinished = false;

    {
        ThreadPool pool(1);

        pool.add(
            [&taskFinished]()
            {
                std::this_thread::sleep_for(200ms);
                taskFinished.store(true);
            });
    }

    ASSERT_TRUE(taskFinished.load());
}

TEST(ThreadPoolTest, SingleTask)
{
    ThreadPool pool(2);
    auto future = pool.add([] { return 42; });
    ASSERT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, MultipleTasks)
{
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i)
    {
        futures.emplace_back(pool.add([i] { return i * i; }));
    }

    for (int i = 0; i < 10; ++i)
    {
        ASSERT_EQ(futures[i].get(), i * i);
    }
}

TEST(ThreadPoolTest, ConcurrentExecution)
{
    ThreadPool pool(4);
    const int numTasks = 1000;
    std::vector<std::future<void>> futures;
    std::atomic<int> counter{0};

    for (int i = 0; i < numTasks; ++i)
    {
        futures.emplace_back(pool.add([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }

    for (auto& future : futures)
    {
        future.get();
    }

    ASSERT_EQ(counter.load(), numTasks);
}