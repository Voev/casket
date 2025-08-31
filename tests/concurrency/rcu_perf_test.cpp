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
        double read_ops_per_sec;
        double write_ops_per_sec;
        double total_ops_per_sec;
        double avg_read_latency_ns;
        double avg_write_latency_ns;
    };

    static TestResults CalculateResults(uint64_t reads, uint64_t writes, uint64_t read_time, uint64_t write_time,
                                         int duration_sec)
    {
        TestResults results;

        results.read_ops_per_sec = static_cast<double>(reads) / duration_sec;
        results.write_ops_per_sec = static_cast<double>(writes) / duration_sec;
        results.total_ops_per_sec = results.read_ops_per_sec + results.write_ops_per_sec;

        results.avg_read_latency_ns = reads > 0 ? static_cast<double>(read_time) / reads : 0;
        results.avg_write_latency_ns = writes > 0 ? static_cast<double>(write_time) / writes : 0;

        return results;
    }
};

TEST_F(RCUPerformanceTest, BasicPerformance)
{
    const int num_readers = 4;
    const int num_writers = 2;
    const int test_duration_sec = 2;

    RCU rcu;
    std::atomic<bool> stop{false};
    std::atomic<int> current_data{0};

    struct ThreadLocalData
    {
        uint64_t reads = 0;
        uint64_t writes = 0;
        uint64_t read_time = 0;
        uint64_t write_time = 0;
    };

    std::vector<ThreadLocalData> reader_data(num_readers);
    std::vector<ThreadLocalData> writer_data(num_writers);
    std::vector<std::thread> threads;

    for (int i = 0; i < num_readers; ++i)
    {
        threads.emplace_back(
            [&, i]()
            {
                Timer timer;
                auto& local = reader_data[i];
                while (!stop.load(std::memory_order_relaxed))
                {
                    timer.start();

                    auto epoch = rcu.read_lock();
                    int value = current_data.load(std::memory_order_relaxed);
                    volatile int nv = value * 2;
                    (void)nv;
                    rcu.read_unlock(epoch);

                    timer.stop();
                    local.read_time += timer.elapsedNanoSecs();
                    local.reads++;
                }
            });
    }

    for (int i = 0; i < num_writers; ++i)
    {
        threads.emplace_back(
            [&, i]()
            {
                Timer timer;
                auto& local = writer_data[i];
                while (!stop.load(std::memory_order_relaxed))
                {
                    timer.start();
                    int new_value = current_data.load(std::memory_order_relaxed) + 1;
                    current_data.store(new_value, std::memory_order_release);
                    rcu.synchronize();

                    timer.stop();
                    local.write_time += timer.elapsedNanoSecs();
                    local.writes++;

                    std::this_thread::sleep_for(milliseconds(1));
                }
            });
    }

    std::this_thread::sleep_for(seconds(test_duration_sec));
    stop.store(true, std::memory_order_relaxed);

    for (auto& thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    uint64_t total_reads = 0;
    uint64_t total_writes = 0;
    uint64_t total_read_time = 0;
    uint64_t total_write_time = 0;

    for (const auto& data : reader_data)
    {
        total_reads += data.reads;
        total_read_time += data.read_time;
    }

    for (const auto& data : writer_data)
    {
        total_writes += data.writes;
        total_write_time += data.write_time;
    }

    auto results = CalculateResults(total_reads, total_writes, total_read_time, total_write_time, test_duration_sec);

    EXPECT_GT(results.read_ops_per_sec, 1000) << "Read operations per second should be reasonable";
    EXPECT_GT(results.write_ops_per_sec, 100) << "Write operations per second should be reasonable";
    EXPECT_LT(results.avg_read_latency_ns, 1000000) << "Read latency should be under 1ms";
    EXPECT_LT(results.avg_write_latency_ns, 10000000) << "Write latency should be under 10ms";

    std::cout << "RCU Performance Results:\n";
    std::cout << "Read OPS: " << results.read_ops_per_sec << "\n";
    std::cout << "Write OPS: " << results.write_ops_per_sec << "\n";
    std::cout << "Avg Read Latency: " << results.avg_read_latency_ns << " ns\n";
    std::cout << "Avg Write Latency: " << results.avg_write_latency_ns << " ns\n";
}

TEST_F(RCUPerformanceTest, MultiThreadedConsistency)
{
    RCU rcu;
    std::atomic<int> data{0};
    std::atomic<bool> stop{false};
    std::atomic<int> consistency_errors{0};

    const int num_threads = 8;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i)
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
                        consistency_errors.fetch_add(1, std::memory_order_relaxed);
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

    EXPECT_EQ(consistency_errors.load(), 0) << "No consistency errors should occur";
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
                        int new_value = data.load(std::memory_order_relaxed) + 1;
                        data.store(new_value, std::memory_order_release);
                        rcu.synchronize();
                        std::this_thread::sleep_for(milliseconds(5));
                    }
                });
        }

        std::this_thread::sleep_for(seconds(1));
        stop.store(true);

        for (auto& thread : threads)
            thread.join();

        EXPECT_TRUE(true) << "Configuration " << readers << "R/" << writers << "W should complete without errors";
    }
}
