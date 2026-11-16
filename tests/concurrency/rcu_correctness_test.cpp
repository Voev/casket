#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <memory>
#include <casket/concurrency/rcu.hpp>
#include <casket/concurrency/rcu_handle.hpp>
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

    constexpr uint32_t kReaderTid = 0;
    constexpr uint32_t kWriterTid = 1;

    rcu.threadRegister(kReaderTid);
    rcu.threadRegister(kWriterTid);

    std::thread reader(
        [&]()
        {
            rcu.threadOnline(kReaderTid);

            while (!stop.load(std::memory_order_acquire))
            {
                auto dataPtr = currentData.get();
                if (!dataPtr.get())
                {
                    continue;
                }

                RCUReadHandle<TestData> handle(dataPtr.get(), rcu, kReaderTid);
                if (!handle)
                {
                    continue;
                }

                const int initialValue = handle->value;
                const std::string initialTimestamp = handle->timestamp;
                const double initialComputed = handle->computed;

                totalReads.fetch_add(1, std::memory_order_relaxed);

                for (int i = 0; i < 10; ++i)
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

                handle.quiescent();
            }

            rcu.threadOffline(kReaderTid);
        });

    std::thread writer(
        [&]()
        {
            rcu.threadOnline(kWriterTid);

            for (int i = 1; i <= 10; ++i)
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

                rcu.synchronize(kWriterTid);

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            rcu.threadOffline(kWriterTid);
            stop.store(true, std::memory_order_release);
        });

    writer.join();
    reader.join();

    rcu.threadUnregister(kReaderTid);
    rcu.threadUnregister(kWriterTid);

    std::cout << "Total reads: " << totalReads.load() << std::endl;
    EXPECT_EQ(inconsistencies.load(), 0) << "Reader must see consistent snapshot. Found " << inconsistencies.load()
                                         << " inconsistencies";
}

TEST_F(RCUCorrectnessTest, SynchronizeWaitsForLongReader)
{
    RCU rcu;
    AtomicSharedPtr<TestData> data = make_atomic_shared<TestData>(0, "x", 0.0);
    constexpr uint32_t kReaderTid = 0, kWriterTid = 1;
    rcu.threadRegister(kReaderTid);
    rcu.threadRegister(kWriterTid);

    std::atomic<bool> readerHolds{false};
    std::thread reader([&]() {
        rcu.threadOnline(kReaderTid);
        readerHolds.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        rcu.quiescent(kReaderTid);
        rcu.threadOffline(kReaderTid);
    });

    while (!readerHolds.load()) std::this_thread::yield();
    rcu.threadOnline(kWriterTid);
    auto t0 = steady_clock::now();
    rcu.synchronize(kWriterTid);
    auto dt = duration_cast<milliseconds>(steady_clock::now() - t0).count();
    rcu.threadOffline(kWriterTid);

    reader.join();
    rcu.threadUnregister(kReaderTid);
    rcu.threadUnregister(kWriterTid);

    EXPECT_GE(dt, 40) << "synchronize must wait for reader";
}

TEST_F(RCUCorrectnessTest, OfflineReaderDoesNotBlock)
{
    RCU rcu;
    constexpr uint32_t kReaderTid = 0, kWriterTid = 1;
    rcu.threadRegister(kReaderTid);
    rcu.threadRegister(kWriterTid);
    rcu.threadOnline(kReaderTid);
    rcu.threadOffline(kReaderTid);

    rcu.threadOnline(kWriterTid);
    auto t0 = steady_clock::now();
    rcu.synchronize(kWriterTid);
    auto dt = duration_cast<milliseconds>(steady_clock::now() - t0).count();
    rcu.threadOffline(kWriterTid);

    rcu.threadUnregister(kReaderTid);
    rcu.threadUnregister(kWriterTid);

    EXPECT_LT(dt, 10) << "offline reader must not block synchronize";
}

TEST_F(RCUCorrectnessTest, TwoWritersNoDeadlock)
{
    RCU rcu;
    constexpr uint32_t kWriterA = 0, kWriterB = 1;
    rcu.threadRegister(kWriterA);
    rcu.threadRegister(kWriterB);

    std::atomic<int> done{0};
    auto writerFn = [&](uint32_t tid) {
        rcu.threadOnline(tid);
        for (int i = 0; i < 100; ++i)
            rcu.synchronize(tid);
        rcu.threadOffline(tid);
        done.fetch_add(1);
    };

    std::thread a(writerFn, kWriterA);
    std::thread b(writerFn, kWriterB);

    a.join();
    b.join();
    rcu.threadUnregister(kWriterA);
    rcu.threadUnregister(kWriterB);

    EXPECT_EQ(done.load(), 2);
}