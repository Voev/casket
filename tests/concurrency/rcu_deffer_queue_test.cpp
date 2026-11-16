#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <casket/concurrency/rcu.hpp>
#include <casket/concurrency/rcu_defer_queue.hpp>

using namespace casket;
using namespace std::chrono;

namespace
{

struct Tracked
{
    static std::atomic<int> alive;
    static std::atomic<int> destroyed;

    int value = 0;

    explicit Tracked(int v = 0)
        : value(v)
    {
        alive.fetch_add(1, std::memory_order_relaxed);
    }
    ~Tracked()
    {
        alive.fetch_sub(1, std::memory_order_relaxed);
        destroyed.fetch_add(1, std::memory_order_relaxed);
    }
};

std::atomic<int> Tracked::alive{0};
std::atomic<int> Tracked::destroyed{0};

void resetTrackedCounters()
{
    Tracked::alive.store(0, std::memory_order_relaxed);
    Tracked::destroyed.store(0, std::memory_order_relaxed);
}

struct TrackedDeleter
{
    void operator()(Tracked* p) const noexcept
    {
        delete p;
    }
};

} // namespace

TEST(RCUDeferQueueTest, FlushFreesAllQueuedObjects)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);

    std::atomic<bool> stop{false};
    std::thread reader(
        [&]()
        {
            rcu.threadOnline(kReaderId);
            while (!stop.load(std::memory_order_relaxed))
                rcu.quiescent(kReaderId);
            rcu.threadOffline(kReaderId);
        });

    RCUDeferQueue<Tracked> dq(rcu, 1000, 100, TrackedDeleter{});

    constexpr int kCount = 10;
    for (int i = 0; i < kCount; ++i)
        dq.enqueue(new Tracked(i));

    EXPECT_EQ(Tracked::alive.load(), kCount);
    EXPECT_EQ(dq.size(), static_cast<std::size_t>(kCount));

    dq.flush();

    stop.store(true, std::memory_order_relaxed);
    reader.join();

    EXPECT_EQ(dq.size(), 0u);
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), kCount);

    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, DestructorDrainsQueue)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);

    {
        RCUDeferQueue<Tracked> dq(rcu, 1000, 100, TrackedDeleter{});
        for (int i = 0; i < 5; ++i)
            dq.enqueue(new Tracked(i));

        rcu.quiescent(kReaderId);

        EXPECT_EQ(Tracked::alive.load(), 5);
    }

    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 5);

    rcu.threadOffline(kReaderId);
    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, NullptrIsIgnored)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);

    RCUDeferQueue<Tracked> dq(rcu, 1000, 100, TrackedDeleter{});
    dq.enqueue(nullptr);

    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(Tracked::destroyed.load(), 0);

    rcu.threadOffline(kReaderId);
    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, ReclaimWaitsForReaderQuiescent)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);

    RCUDeferQueue<Tracked> dq(rcu, 1000, 100, TrackedDeleter{});
    dq.enqueue(new Tracked(1));
    EXPECT_EQ(Tracked::alive.load(), 1);

    const std::size_t freed = dq.reclaim();
    EXPECT_EQ(freed, 0u);
    EXPECT_EQ(Tracked::alive.load(), 1);

    rcu.quiescent(kReaderId);

    EXPECT_EQ(dq.reclaim(), 1u);
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 1);

    rcu.threadOffline(kReaderId);
    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, OfflineReaderDoesNotBlockReclaim)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);
    rcu.threadOffline(kReaderId);

    RCUDeferQueue<Tracked> dq(rcu, 1000, 100, TrackedDeleter{});
    dq.enqueue(new Tracked(1));

    EXPECT_EQ(dq.reclaim(), 1u);
    EXPECT_EQ(Tracked::alive.load(), 0);

    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, ReclaimStopsAtFirstBlockedEntry)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);

    RCUDeferQueue<Tracked> dq(rcu, 1000, 100, TrackedDeleter{});

    dq.enqueue(new Tracked(1));
    rcu.quiescent(kReaderId);
    EXPECT_EQ(dq.reclaim(), 1u);
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 1);

    dq.enqueue(new Tracked(2)); // token_ = 2, qsbrCnt_[0] = 1
    dq.enqueue(new Tracked(3)); // token_ = 3, qsbrCnt_[0] = 1

    EXPECT_EQ(dq.reclaim(), 0u);
    EXPECT_EQ(Tracked::alive.load(), 2);
    EXPECT_EQ(dq.size(), 2u);

    rcu.quiescent(kReaderId); // qsbrCnt_[0] = 3
    EXPECT_EQ(dq.reclaim(), 2u);
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 3);
    EXPECT_TRUE(dq.empty());

    rcu.threadOffline(kReaderId);
    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, MaxReclaimLimitsFreePerCall)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);

    constexpr std::size_t kMaxReclaim = 3;
    RCUDeferQueue<Tracked> dq(rcu, 1000, kMaxReclaim, TrackedDeleter{});

    for (int i = 0; i < 10; ++i)
        dq.enqueue(new Tracked(i));
    rcu.quiescent(kReaderId);

    EXPECT_EQ(Tracked::alive.load(), 10);

    EXPECT_EQ(dq.reclaim(), kMaxReclaim);
    EXPECT_EQ(Tracked::alive.load(), 7);
    EXPECT_EQ(dq.reclaim(), kMaxReclaim);
    EXPECT_EQ(Tracked::alive.load(), 4);
    EXPECT_EQ(dq.reclaim(), kMaxReclaim);
    EXPECT_EQ(Tracked::alive.load(), 1);
    EXPECT_EQ(dq.reclaim(), 1u);
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 10);

    rcu.threadOffline(kReaderId);
    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, TriggerLimitReclaimsInline)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);
    rcu.threadOffline(kReaderId);

    constexpr std::size_t kTrigger = 4;
    constexpr std::size_t kMaxReclaim = 2;
    RCUDeferQueue<Tracked> dq(rcu, kTrigger, kMaxReclaim, TrackedDeleter{});

    for (int i = 0; i < 3; ++i)
        dq.enqueue(new Tracked(i));
    EXPECT_EQ(Tracked::alive.load(), 3);
    EXPECT_EQ(dq.size(), 3u);

    dq.enqueue(new Tracked(3));
    EXPECT_EQ(Tracked::destroyed.load(), 2);
    EXPECT_EQ(dq.size(), 2u);

    dq.flush();
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 4);

    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, TriggerDoesNotFreeWhileReaderBlocked)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);

    constexpr std::size_t kTrigger = 2;
    RCUDeferQueue<Tracked> dq(rcu, kTrigger, 10, TrackedDeleter{});

    dq.enqueue(new Tracked(1));
    dq.enqueue(new Tracked(2));

    EXPECT_EQ(Tracked::destroyed.load(), 0);
    EXPECT_EQ(Tracked::alive.load(), 2);

    rcu.quiescent(kReaderId);
    dq.enqueue(new Tracked(3));

    EXPECT_EQ(Tracked::destroyed.load(), 2);
    EXPECT_EQ(Tracked::alive.load(), 1);

    rcu.quiescent(kReaderId);
    dq.flush();
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 3);

    rcu.threadOffline(kReaderId);
    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, ZeroLimitsAreClamped)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);
    rcu.threadOnline(kReaderId);
    rcu.threadOffline(kReaderId);

    RCUDeferQueue<Tracked> dq(rcu, 0, 0, TrackedDeleter{});

    for (int i = 0; i < 5; ++i)
        dq.enqueue(new Tracked(i));

    dq.flush();
    EXPECT_EQ(Tracked::alive.load(), 0);
    EXPECT_EQ(Tracked::destroyed.load(), 5);

    rcu.threadUnregister(kReaderId);
}

TEST(RCUDeferQueueTest, ReclaimWaitsForConcreteSnapshot)
{
    resetTrackedCounters();
    RCU rcu;
    constexpr uint32_t kReaderId = 0;
    rcu.threadRegister(kReaderId);

    std::atomic<Tracked*> shared{new Tracked(0)};
    RCUDeferQueue<Tracked> dq(rcu, 1000, 100, TrackedDeleter{});

    std::atomic<bool> readerHoldsSnapshot{false};
    std::atomic<bool> readerReleased{false};
    std::atomic<bool> readerSawExpectedValue{false};
    std::atomic<bool> stop{false};

    // Reader thread: acquires a raw snapshot, holds it until the writer
    // signals, then publishes its quiescent state.
    std::thread reader([&]() {
        rcu.threadOnline(kReaderId);

        // Acquire a raw pointer through RCU. The writer must not free it
        // until this thread calls quiescent().
        Tracked* snapshot = shared.load(std::memory_order_acquire);
        readerSawExpectedValue.store(snapshot->value == 0,
                                     std::memory_order_relaxed);
        readerHoldsSnapshot.store(true, std::memory_order_release);

        // Hold the snapshot until the writer has enqueued the old pointer
        // and asked us to release.
        while (!stop.load(std::memory_order_acquire))
            std::this_thread::yield();

        // Release the snapshot: after this point the writer may reclaim it.
        rcu.quiescent(kReaderId);
        readerReleased.store(true, std::memory_order_release);

        rcu.threadOffline(kReaderId);
    });

    // Writer (main thread): wait until the reader actually holds the snapshot,
    // then publish a new pointer and enqueue the old one.
    while (!readerHoldsSnapshot.load(std::memory_order_acquire))
        std::this_thread::yield();

    Tracked* old = shared.exchange(new Tracked(1), std::memory_order_acq_rel);
    dq.enqueue(old);

    // Both the old (held by the reader) and the new object are alive.
    EXPECT_EQ(Tracked::alive.load(), 2);

    // reclaim() must NOT free the old pointer: the reader has not yet
    // published a quiescent state, so its snapshot is still in use.
    EXPECT_EQ(dq.reclaim(), 0u);
    EXPECT_EQ(Tracked::alive.load(), 2);

    // Allow the reader to release its snapshot.
    stop.store(true, std::memory_order_release);

    // Wait until the reader has actually called quiescent().
    while (!readerReleased.load(std::memory_order_acquire))
        std::this_thread::yield();

    reader.join();

    // The reader is now offline; reclaim() must free the old pointer.
    EXPECT_EQ(dq.reclaim(), 1u);
    EXPECT_EQ(Tracked::alive.load(), 1);   // only the new object remains
    EXPECT_EQ(Tracked::destroyed.load(), 1);

    dq.flush();

    delete shared.load(std::memory_order_relaxed);
    rcu.threadUnregister(kReaderId);
}