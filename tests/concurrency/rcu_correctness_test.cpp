#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include "casket/concurrency/rcu.hpp"

using namespace casket;
using namespace std::chrono;

class RCUCorrectnessTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        inconsistencies.store(0);
        totalReads.store(0);
        stop.store(false);
    }

    std::atomic<int> inconsistencies{0};
    std::atomic<int> totalReads{0};
    std::atomic<bool> stop{false};
};

struct TestData
{
    int value;
    std::string timestamp;
    double computed;
};

struct DataHolder
{
    std::atomic<TestData*> ptr{nullptr};

    ~DataHolder()
    {
        delete ptr.load(std::memory_order_relaxed);
    }
};

TEST_F(RCUCorrectnessTest, ViewSnapshots)
{
    RCU rcu;

    TestData* currentData = new TestData{0, "start", 0.0};
    std::atomic<TestData*> currentPtr;

    currentPtr = currentData;

    DataHolder holder;
    holder.ptr.store(new TestData{0, "start", 0.0}, std::memory_order_release);

    std::thread reader(
        [&]()
        {
            while (!stop.load())
            {
                TestData* dataPtr = holder.ptr.load(std::memory_order_acquire);
                if (!dataPtr)
                {
                    continue;
                }

                RCUReadHandle<TestData> handle(dataPtr, rcu);
                if (!handle)
                {
                    continue;
                }

                TestData snapshot = *handle;
                for (int i = 0; i < 100; ++i)
                {
                    if (handle->value != snapshot.value || handle->timestamp != snapshot.timestamp ||
                        handle->computed != snapshot.computed)
                    {
                        inconsistencies.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        });

    std::thread writer(
        [&]()
        {
            for (int i = 1; i <= 100; ++i)
            {
                TestData* newData = new TestData{i, "update", i * 1.0};
                TestData* oldData = holder.ptr.exchange(newData, std::memory_order_acq_rel);

                rcu.synchronize();
                delete oldData;

                std::this_thread::sleep_for(milliseconds(1));
            }
            stop.store(true);
        });

    writer.join();
    reader.join();

    EXPECT_EQ(inconsistencies.load(), 0) << "Reader must see consistent snapshot. Found " << inconsistencies.load()
                                         << " inconsistencies";
}