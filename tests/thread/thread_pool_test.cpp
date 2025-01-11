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

    auto task = [&counter]() {
        std::this_thread::sleep_for(50ms);
        ++counter;
    };

    for (int i = 0; i < 10; ++i)
    {
        pool.addTask(task);
    }

    std::this_thread::sleep_for(1s);

    EXPECT_EQ(counter.load(), 10);
}

TEST(ThreadPoolTest, AddTask)
{
    ThreadPool pool(2);
    std::atomic<int> result = 0;

    pool.addTask([&result]() {
        std::this_thread::sleep_for(100ms);
        result.fetch_add(1);
    });

    pool.addTask([&result]() {
        std::this_thread::sleep_for(150ms);
        result.fetch_add(2);
    });

    std::this_thread::sleep_for(300ms);

    EXPECT_EQ(result.load(), 3);
}


TEST(ThreadPoolTest, Add)
{
    ThreadPool pool(2);
    std::atomic<int> result = 0;

    pool.add([&result]() {
        std::this_thread::sleep_for(100ms);
        result.fetch_add(1);
    });

    pool.add([&result]() {
        std::this_thread::sleep_for(150ms);
        result.fetch_add(2);
    });

    std::this_thread::sleep_for(300ms);

    EXPECT_EQ(result.load(), 3);
}

TEST(ThreadPoolTest, DestructorWaitsForTasks)
{
    std::atomic<bool> taskFinished = false;

    {
        ThreadPool pool(1);

        pool.addTask([&taskFinished]() {
            std::this_thread::sleep_for(200ms);
            taskFinished.store(true);
        });
    }

    EXPECT_TRUE(taskFinished.load());
}
