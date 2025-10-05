#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <memory>
#include <casket/concurrency/rcu.hpp>
#include <casket/lock_free/atomic_shared_ptr.hpp>

using namespace casket;
using namespace casket::lock_free;
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

TEST_F(RCUCorrectnessTest, ViewSnapshots)
{
    RCU rcu;

    AtomicSharedPtr<TestData> currentData = make_atomic_shared<TestData>(0, "start", 0.0);

    std::thread reader(
        [&]()
        {
            while (!stop.load())
            {
                auto dataPtr = currentData.get();
                if (!dataPtr.get())
                {
                    continue;
                }

                RCUReadHandle<TestData> handle(dataPtr.get(), rcu);
                if (!handle)
                {
                    continue;
                }

                const int initialValue = handle->value;
                const std::string initialTimestamp = handle->timestamp;
                const double initialComputed = handle->computed;

                totalReads.fetch_add(1, std::memory_order_relaxed);

                for (int i = 0; i < 100; ++i)
                {
                    if (handle->value != initialValue || handle->timestamp != initialTimestamp ||
                        handle->computed != initialComputed)
                    {
                        inconsistencies.fetch_add(1, std::memory_order_relaxed);
                        std::cout << "[INCONSISTENCY] Thread " << std::this_thread::get_id()
                                  << " - Initial: " << initialValue << ", Current: " << handle->value
                                  << ", Epoch: " << rcu.getEpoch() << std::endl;
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
                auto newData = make_shared<TestData>(i, "update", i * 1.0);

                while (true)
                {
                    auto current = currentData.get();
                    if (currentData.compareExchange(current.get(), std::move(newData)))
                    {
                        break;
                    }
                }
                
                rcu.synchronize();

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            stop.store(true, std::memory_order_release);
        });

    writer.join();
    reader.join();

    std::cout << "Total reads: " << totalReads.load() << std::endl;
    EXPECT_EQ(inconsistencies.load(), 0) << "Reader must see consistent snapshot. Found " << inconsistencies.load()
                                         << " inconsistencies";
}