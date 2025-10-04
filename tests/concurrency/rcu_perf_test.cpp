#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <casket/concurrency/rcu.hpp>
#include <casket/utils/timer.hpp>

using namespace casket;
using namespace std::chrono;

class RCUPerformanceTest : public ::testing::Test
{
protected:
    struct TestResults
    {
        double readOpsPerSec;
        double writeOpsPerSec;
        double totalOpsPerSec;
        double avgReadLatencyNs;
        double avgWriteLatencyNs;
    };

    static TestResults CalculateResults(uint64_t reads, uint64_t writes, uint64_t readTime, uint64_t writeTime,
                                         int durationSec)
    {
        TestResults results;

        results.readOpsPerSec = static_cast<double>(reads) / durationSec;
        results.writeOpsPerSec = static_cast<double>(writes) / durationSec;
        results.totalOpsPerSec = results.readOpsPerSec + results.writeOpsPerSec;

        results.avgReadLatencyNs = reads > 0 ? static_cast<double>(readTime) / reads : 0;
        results.avgWriteLatencyNs = writes > 0 ? static_cast<double>(writeTime) / writes : 0;

        return results;
    }
};

TEST_F(RCUPerformanceTest, BasicPerformance)
{
    const int numReaders = 4;
    const int numWriters = 2;
    const int testDurationSec = 2;

    RCU rcu;
    std::atomic<bool> stop{false};
    std::atomic<int> currentData{0};

    struct ThreadLocalData
    {
        uint64_t reads = 0;
        uint64_t writes = 0;
        uint64_t readTime = 0;
        uint64_t writeTime = 0;
    };

    std::vector<ThreadLocalData> readerData(numReaders);
    std::vector<ThreadLocalData> writerData(numWriters);
    std::vector<std::thread> threads;

    for (int i = 0; i < numReaders; ++i)
    {
        threads.emplace_back(
            [&, i]()
            {
                Timer timer;
                auto& local = readerData[i];
                while (!stop.load(std::memory_order_relaxed))
                {
                    timer.start();

                    auto epoch = rcu.read_lock();
                    int value = currentData.load(std::memory_order_relaxed);
                    volatile int nv = value * 2;
                    (void)nv;
                    rcu.read_unlock(epoch);

                    timer.stop();
                    local.readTime += timer.elapsedNanoSecs();
                    local.reads++;
                }
            });
    }

    for (int i = 0; i < numWriters; ++i)
    {
        threads.emplace_back(
            [&, i]()
            {
                Timer timer;
                auto& local = writerData[i];
                while (!stop.load(std::memory_order_relaxed))
                {
                    timer.start();
                    int newValue = currentData.load(std::memory_order_relaxed) + 1;
                    currentData.store(newValue, std::memory_order_release);
                    rcu.synchronize();

                    timer.stop();
                    local.writeTime += timer.elapsedNanoSecs();
                    local.writes++;

                    std::this_thread::sleep_for(milliseconds(1));
                }
            });
    }

    std::this_thread::sleep_for(seconds(testDurationSec));
    stop.store(true, std::memory_order_relaxed);

    for (auto& thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    uint64_t totalReads = 0;
    uint64_t totalWrites = 0;
    uint64_t totalReadTime = 0;
    uint64_t totalWriteTime = 0;

    for (const auto& data : readerData)
    {
        totalReads += data.reads;
        totalReadTime += data.readTime;
    }

    for (const auto& data : writerData)
    {
        totalWrites += data.writes;
        totalWriteTime += data.writeTime;
    }

    auto results = CalculateResults(totalReads, totalWrites, totalReadTime, totalWriteTime, testDurationSec);

    EXPECT_GT(results.readOpsPerSec, 1000) << "Read operations per second should be reasonable";
    EXPECT_GT(results.writeOpsPerSec, 100) << "Write operations per second should be reasonable";
    EXPECT_LT(results.avgReadLatencyNs, 1000000) << "Read latency should be under 1ms";
    EXPECT_LT(results.avgWriteLatencyNs, 10000000) << "Write latency should be under 10ms";

    std::cout << "RCU Performance Results:\n";
    std::cout << "Read OPS: " << results.readOpsPerSec << "\n";
    std::cout << "Write OPS: " << results.writeOpsPerSec << "\n";
    std::cout << "Avg Read Latency: " << results.avgReadLatencyNs << " ns\n";
    std::cout << "Avg Write Latency: " << results.avgWriteLatencyNs << " ns\n";
}

TEST_F(RCUPerformanceTest, MultiThreadedConsistency)
{
    RCU rcu;
    std::atomic<int> data{0};
    std::atomic<bool> stop{false};
    std::atomic<int> consistencyErrors{0};

    const int numThreads = 8;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(
            [&]()
            {
                while (!stop.load(std::memory_order_relaxed))
                {
                    auto epoch = rcu.read_lock();
                    int value = data.load(std::memory_order_acquire);

                    if (value < 0)
                    {
                        consistencyErrors.fetch_add(1, std::memory_order_relaxed);
                    }

                    rcu.read_unlock(epoch);
                }
            });
    }

    std::thread writer(
        [&]()
        {
            for (int i = 0; i < 1000; ++i)
            {
                data.store(i + 1, std::memory_order_release);
                rcu.synchronize();
                std::this_thread::sleep_for(microseconds(100));
            }
            stop.store(true);
        });

    for (auto& thread : threads)
    {
        thread.join();
    }
    writer.join();

    EXPECT_EQ(consistencyErrors.load(), 0) << "No consistency errors should occur";
}

TEST_F(RCUPerformanceTest, DifferentThreadConfigurations)
{
    std::vector<std::pair<int, int>> configurations = {
        {1, 1}, {2, 1}, {4, 1}, {8, 1},
        {1, 2}, {2, 2}, {4, 2}, {8, 2}
    };

    for (const auto& [readers, writers] : configurations)
    {
        std::cout << "Testing configuration: " << readers << " readers, " << writers << " writers\n";

        RCU rcu;
        std::atomic<bool> stop{false};
        std::atomic<int> data{0};

        std::vector<std::thread> threads;

        for (int i = 0; i < readers; ++i)
        {
            threads.emplace_back(
                [&]()
                {
                    while (!stop.load())
                    {
                        auto epoch = rcu.read_lock();
                        volatile int value = data.load(std::memory_order_relaxed);
                        (void)value;
                        rcu.read_unlock(epoch);
                    }
                });
        }

        for (int i = 0; i < writers; ++i)
        {
            threads.emplace_back(
                [&]()
                {
                    while (!stop.load())
                    {
                        int newValue = data.load(std::memory_order_relaxed) + 1;
                        data.store(newValue, std::memory_order_release);
                        rcu.synchronize();
                        std::this_thread::sleep_for(milliseconds(5));
                    }
                });
        }

        std::this_thread::sleep_for(seconds(1));
        stop.store(true);

        for (auto& thread : threads)
        {
            thread.join();
        }

        EXPECT_TRUE(true) << "Configuration " << readers << "R/" << writers << "W should complete without errors";
    }
}
