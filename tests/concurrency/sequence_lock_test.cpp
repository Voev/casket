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
    int value = lock.load();

    EXPECT_EQ(value, 42);
}

TEST_F(SequenceLockTest, MultipleStoreLoad)
{
    SequenceLock<int> lock;

    for (int i = 0; i < 100; ++i)
    {
        lock.store(i);
        int value = lock.load();
        EXPECT_EQ(value, i);
    }
}

TEST_F(SequenceLockTest, DifferentTypes)
{
    // Test with double
    SequenceLock<double> double_lock;
    double_lock.store(3.14159);
    double dvalue = double_lock.load();
    EXPECT_DOUBLE_EQ(dvalue, 3.14159);

    // Test with custom struct
    struct Point
    {
        int x, y;
    };
    SequenceLock<Point> point_lock;
    point_lock.store(Point{10, 20});
    Point pvalue = point_lock.load();
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

    LargeData loaded = lock.load();

    EXPECT_EQ(loaded.value, original.value);
    EXPECT_EQ(memcmp(loaded.data, original.data, sizeof(original.data)), 0);
}

TEST_F(SequenceLockTest, SingleWriterSingleReader)
{
    SequenceLock<int> lock;
    std::atomic<int> writer_count{0};

    std::thread writer([&]() {
        for (int i = 0; i < 10000; ++i) {
            lock.store(i);
            writer_count.store(i + 1, std::memory_order_release);
        }
    });

    std::thread reader([&]() {
        int last_value = -1;
        while (writer_count.load(std::memory_order_acquire) < 10000) {
            int value = lock.load();
            EXPECT_GE(value, last_value) << "Sequence violation!";
            last_value = value;
        }
    });

    writer.join();
    reader.join();

    EXPECT_EQ(writer_count.load(), 10000);
}

TEST_F(SequenceLockTest, RapidStoreLoad)
{
    SequenceLock<int> lock;

    for (int i = 0; i < 10000; ++i)
    {
        lock.store(i);
        int value = lock.load();
        EXPECT_EQ(value, i);
    }
}

