#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include <mutex>

#include <casket/concurrency/sequence_lock.hpp>

using namespace casket;

class SequenceLockTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

TEST_F(SequenceLockTest, BasicStoreLoad)
{
    SequenceLock<int> lock;

    lock.store(42);
    int value = 0;
    lock.load(value);

    EXPECT_EQ(value, 42);
}

TEST_F(SequenceLockTest, MultipleStoreLoad)
{
    SequenceLock<int> lock;

    for (int i = 0; i < 100; ++i)
    {
        lock.store(i);
        int value = 0;
        lock.load(value);
        EXPECT_EQ(value, i);
    }
}

TEST_F(SequenceLockTest, DifferentTypes)
{
    // Test with double
    SequenceLock<double> double_lock;
    double_lock.store(3.14159);
    double dvalue = 0;
    double_lock.load(dvalue);
    EXPECT_DOUBLE_EQ(dvalue, 3.14159);

    // Test with custom struct
    struct Point
    {
        int x, y;
    };
    SequenceLock<Point> point_lock;
    point_lock.store(Point{10, 20});
    Point pvalue{0, 0};
    point_lock.load(pvalue);
    EXPECT_EQ(pvalue.x, 10);
    EXPECT_EQ(pvalue.y, 20);
}

TEST_F(SequenceLockTest, LargeObject)
{
    struct LargeData
    {
        char data[256];
        int value;
    };

    SequenceLock<LargeData> lock;
    LargeData original;
    original.value = 12345;
    memset(original.data, 'A', sizeof(original.data));

    lock.store(original);

    LargeData loaded;
    lock.load(loaded);

    EXPECT_EQ(loaded.value, original.value);
    EXPECT_EQ(memcmp(loaded.data, original.data, sizeof(original.data)), 0);
}

TEST_F(SequenceLockTest, SingleWriterSingleReader)
{
    SequenceLock<int> lock;
    std::atomic<bool> running{true};
    std::atomic<int> writer_count{0};
    std::atomic<int> reader_count{0};

    // Writer thread
    std::thread writer(
        [&]()
        {
            for (int i = 0; i < 10000 && running; ++i)
            {
                lock.store(i);
                writer_count++;
            }
        });

    // Reader thread
    std::thread reader(
        [&]()
        {
            int last_value = -1;
            while (reader_count < 10000)
            {
                int value;
                lock.load(value);
                EXPECT_GE(value, last_value) << "Sequence violation!";
                last_value = value;
                reader_count++;
            }
            (void)last_value;
            running = false;
        });

    writer.join();
    reader.join();

    EXPECT_EQ(writer_count.load(), 10000);
    EXPECT_EQ(reader_count.load(), 10000);
}

TEST_F(SequenceLockTest, SingleWriterMultipleReaders)
{
    SequenceLock<int> lock;
    std::atomic<bool> running{true};
    std::atomic<int> writer_count{0};
    std::atomic<int> total_reads{0};
    constexpr int NUM_READERS = 4;
    constexpr int NUM_WRITES = 5000;

    // Writer thread
    std::thread writer(
        [&]()
        {
            for (int i = 0; i < NUM_WRITES && running; ++i)
            {
                lock.store(i);
                writer_count++;
            }
            running = false; // Signal readers to stop
        });

    // Reader threads
    std::vector<std::thread> readers;
    std::atomic<bool> sequence_error{false};
    std::atomic<int> readers_complete{0};

    for (int r = 0; r < NUM_READERS; ++r)
    {
        readers.emplace_back(
            [&, r]()
            {
                int last_value = -1;
                while (running)
                {
                    int value;
                    lock.load(value);
                    if (value < last_value)
                    {
                        sequence_error = true;
                    }
                    last_value = value;
                    total_reads++;
                }
                readers_complete++;
            });
    }

    writer.join();

    // Wait for all readers to finish
    while (readers_complete < NUM_READERS)
    {
        std::this_thread::yield();
    }

    for (auto& th : readers)
        th.join();

    EXPECT_FALSE(sequence_error.load()) << "Sequence violation detected";
    EXPECT_EQ(writer_count.load(), NUM_WRITES);

    // Readers can have different counts, just check they read something
    EXPECT_GT(total_reads.load(), 0);
    std::cout << "Total reads: " << total_reads.load() << std::endl;
}

TEST_F(SequenceLockTest, DefaultConstructed)
{
    SequenceLock<int> lock;
    int value = 0;
    lock.load(value);
    // Default constructed value is indeterminate, just ensure no crash
}

TEST_F(SequenceLockTest, RapidStoreLoad)
{
    SequenceLock<int> lock;

    for (int i = 0; i < 10000; ++i)
    {
        lock.store(i);
        int value = -1;
        lock.load(value);
        EXPECT_EQ(value, i);
    }
}

TEST_F(SequenceLockTest, ConcurrentStoreSameValue)
{
    SequenceLock<int> lock;
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int t = 0; t < 10; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                while (!start) {}
                for (int i = 0; i < 1000; ++i)
                {
                    lock.store(t); // Different writers without sync - not recommended
                }
            });
    }

    std::thread reader(
        [&]()
        {
            while (!start) {}
            for (int i = 0; i < 10000; ++i)
            {
                int value;
                lock.load(value);
                // Reader might see any value, just verify it's in range
                if (value < 0 || value >= 10)
                {
                    errors++;
                }
            }
        });

    start = true;

    for (auto& th : threads)
        th.join();
    reader.join();

    EXPECT_EQ(errors.load(), 0);
}
