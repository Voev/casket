#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <array>
#include <memory>
#include <casket/concurrency/read_write_lock.hpp>

using namespace casket;

class ReadWriteLockTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

TEST_F(ReadWriteLockTest, DefaultConstructor_ShouldBeUnlocked)
{
    ReadWriteLock lock;

    ASSERT_FALSE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());
    ASSERT_EQ(lock.readerCount(), 0);
}

TEST_F(ReadWriteLockTest, MoveConstructor_ShouldTransferState)
{
    ReadWriteLock lock1;

    lock1.readLock();
    ASSERT_EQ(lock1.readerCount(), 1);

    ReadWriteLock lock2(std::move(lock1));

    ASSERT_EQ(lock1.readerCount(), 0);
    ASSERT_FALSE(lock1.isLockedForRead());

    ASSERT_EQ(lock2.readerCount(), 1);
    ASSERT_TRUE(lock2.isLockedForRead());

    lock2.readUnlock();
}

TEST_F(ReadWriteLockTest, MoveAssignment_ShouldTransferState)
{
    ReadWriteLock lock1;
    ReadWriteLock lock2;

    lock1.writeLock();
    ASSERT_TRUE(lock1.isLockedForWrite());

    lock2 = std::move(lock1);

    ASSERT_FALSE(lock1.isLockedForWrite());
    ASSERT_TRUE(lock2.isLockedForWrite());

    lock2.writeUnlock();
}

TEST_F(ReadWriteLockTest, ReadLock_ShouldAllowMultipleReaders)
{
    ReadWriteLock lock;

    lock.readLock();
    ASSERT_EQ(lock.readerCount(), 1);
    ASSERT_TRUE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());

    lock.readLock();
    ASSERT_EQ(lock.readerCount(), 2);
    ASSERT_TRUE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());

    lock.readLock();
    ASSERT_EQ(lock.readerCount(), 3);
    ASSERT_TRUE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());

    lock.readUnlock();
    ASSERT_EQ(lock.readerCount(), 2);
    ASSERT_TRUE(lock.isLockedForRead());

    lock.readUnlock();
    ASSERT_EQ(lock.readerCount(), 1);
    ASSERT_TRUE(lock.isLockedForRead());

    lock.readUnlock();
    ASSERT_EQ(lock.readerCount(), 0);
    ASSERT_FALSE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, ReadLock_ShouldBlockWhenWriteLockHeld)
{
    ReadWriteLock lock;
    std::atomic<bool> reader_started{false};
    std::atomic<bool> reader_finished{false};

    lock.writeLock();
    ASSERT_TRUE(lock.isLockedForWrite());

    std::thread reader(
        [&]()
        {
            reader_started = true;
            lock.readLock();
            reader_finished = true;
            lock.readUnlock();
        });

    while (!reader_started)
    {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_FALSE(reader_finished);
    ASSERT_TRUE(lock.isLockedForWrite());

    lock.writeUnlock();
    reader.join();

    ASSERT_TRUE(reader_finished);
    ASSERT_FALSE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, WriteLock_ShouldBeExclusive)
{
    ReadWriteLock lock;

    lock.writeLock();
    ASSERT_FALSE(lock.isLockedForRead());
    ASSERT_TRUE(lock.isLockedForWrite());
    ASSERT_EQ(lock.readerCount(), 0);

    lock.writeUnlock();
    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, WriteLock_ShouldBlockWhenReadersActive)
{
    ReadWriteLock lock;
    std::atomic<bool> writer_started{false};
    std::atomic<bool> writer_finished{false};

    lock.readLock();
    ASSERT_EQ(lock.readerCount(), 1);

    std::thread writer(
        [&]()
        {
            writer_started = true;
            lock.writeLock();
            writer_finished = true;
            lock.writeUnlock();
        });

    while (!writer_started)
    {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_FALSE(writer_finished);
    ASSERT_TRUE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());

    lock.readUnlock();
    writer.join();

    ASSERT_TRUE(writer_finished);
    ASSERT_FALSE(lock.isLockedForRead());
    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, WriteLock_ShouldBlockOtherWriters)
{
    ReadWriteLock lock;
    std::atomic<bool> writer2_started{false};
    std::atomic<bool> writer2_finished{false};

    lock.writeLock();
    ASSERT_TRUE(lock.isLockedForWrite());

    std::thread writer2(
        [&]()
        {
            writer2_started = true;
            lock.writeLock();
            writer2_finished = true;
            lock.writeUnlock();
        });

    while (!writer2_started)
    {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_FALSE(writer2_finished);
    ASSERT_TRUE(lock.isLockedForWrite());

    lock.writeUnlock();
    writer2.join();

    ASSERT_TRUE(writer2_finished);
    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, TryReadLock_ShouldSucceedWhenNoWriter)
{
    ReadWriteLock lock;

    ASSERT_TRUE(lock.tryReadLock());
    ASSERT_EQ(lock.readerCount(), 1);
    ASSERT_TRUE(lock.isLockedForRead());

    ASSERT_TRUE(lock.tryReadLock());
    ASSERT_EQ(lock.readerCount(), 2);

    lock.readUnlock();
    lock.readUnlock();
}

TEST_F(ReadWriteLockTest, TryReadLock_ShouldFailWhenWriterActive)
{
    ReadWriteLock lock;

    lock.writeLock();
    ASSERT_TRUE(lock.isLockedForWrite());

    ASSERT_FALSE(lock.tryReadLock());
    ASSERT_EQ(lock.readerCount(), 0);
    ASSERT_FALSE(lock.isLockedForRead());

    lock.writeUnlock();

    ASSERT_TRUE(lock.tryReadLock());
    lock.readUnlock();
}

TEST_F(ReadWriteLockTest, TryWriteLock_ShouldSucceedWhenUnlocked)
{
    ReadWriteLock lock;

    ASSERT_TRUE(lock.tryWriteLock());
    ASSERT_TRUE(lock.isLockedForWrite());

    lock.writeUnlock();
}

TEST_F(ReadWriteLockTest, TryWriteLock_ShouldFailWhenReadersActive)
{
    ReadWriteLock lock;

    lock.readLock();
    ASSERT_EQ(lock.readerCount(), 1);

    ASSERT_FALSE(lock.tryWriteLock());
    ASSERT_FALSE(lock.isLockedForWrite());

    lock.readUnlock();

    ASSERT_TRUE(lock.tryWriteLock());
    lock.writeUnlock();
}

TEST_F(ReadWriteLockTest, TryWriteLock_ShouldFailWhenWriterActive)
{
    ReadWriteLock lock;

    lock.writeLock();
    ASSERT_TRUE(lock.isLockedForWrite());

    ASSERT_FALSE(lock.tryWriteLock());
    ASSERT_TRUE(lock.isLockedForWrite());

    lock.writeUnlock();
}

TEST_F(ReadWriteLockTest, Concurrent_ThreeReaders_ShouldReadSimultaneously)
{
    ReadWriteLock lock;
    std::atomic<int> counter{0};
    std::vector<std::thread> readers;
    std::array<bool, 3> reader_completed{false, false, false};

    for (int i = 0; i < 3; ++i)
    {
        readers.emplace_back(
            [&, i]()
            {
                lock.readLock();
                int current = counter.load();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ASSERT_EQ(counter.load(), current);
                reader_completed[i] = true;
                lock.readUnlock();
            });
    }

    for (auto& t : readers)
    {
        t.join();
    }

    for (bool completed : reader_completed)
    {
        ASSERT_TRUE(completed);
    }
}

TEST_F(ReadWriteLockTest, Concurrent_OneWriter_ShouldWriteExclusively)
{
    ReadWriteLock lock;
    std::atomic<int> shared_data{0};
    const int iterations = 100;

    std::thread writer(
        [&]()
        {
            for (int i = 0; i < iterations; ++i)
            {
                lock.writeLock();
                shared_data++;
                lock.writeUnlock();
            }
        });

    writer.join();

    ASSERT_EQ(shared_data, iterations);
}

TEST_F(ReadWriteLockTest, Concurrent_ThreeReadersAndOneWriter_ShouldBeConsistent)
{
    ReadWriteLock lock;
    std::atomic<int> shared_data{0};
    std::atomic<bool> stop{false};
    std::vector<int> reader_readings[3];

    std::thread writer(
        [&]()
        {
            for (int i = 0; i < 50; ++i)
            {
                lock.writeLock();
                shared_data++;
                lock.writeUnlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            stop = true;
        });

    std::vector<std::thread> readers;
    for (int r = 0; r < 3; ++r)
    {
        readers.emplace_back(
            [&, r]()
            {
                while (!stop)
                {
                    lock.readLock();
                    int value = shared_data.load();
                    reader_readings[r].push_back(value);
                    lock.readUnlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            });
    }

    writer.join();
    stop = true;

    for (auto& t : readers)
    {
        t.join();
    }

    for (int r = 0; r < 3; ++r)
    {
        for (size_t i = 1; i < reader_readings[r].size(); ++i)
        {
            ASSERT_GE(reader_readings[r][i], reader_readings[r][i - 1]);
            ASSERT_LE(reader_readings[r][i] - reader_readings[r][i - 1], 2);
        }
    }
}

TEST_F(ReadWriteLockTest, Concurrent_ReaderWriterPriority_ReaderShouldNotBlockWriterIndefinitely)
{
    ReadWriteLock lock;
    std::atomic<int> writer_count{0};
    std::atomic<bool> writer_running{false};
    const int expected_writes = 10;

    std::thread reader_thread(
        [&]()
        {
            for (int i = 0; i < 100; ++i)
            {
                lock.readLock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                lock.readUnlock();
                std::this_thread::yield();
            }
        });

    std::thread writer_thread(
        [&]()
        {
            for (int i = 0; i < expected_writes; ++i)
            {
                writer_running = true;
                lock.writeLock();
                writer_count++;
                lock.writeUnlock();
                writer_running = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });

    writer_thread.join();
    reader_thread.join();

    ASSERT_EQ(writer_count, expected_writes);
}

TEST_F(ReadWriteLockTest, Concurrent_TryMethods_ShouldNotBlock)
{
    ReadWriteLock lock;
    std::atomic<int> success_count{0};

    lock.writeLock();

    std::array<std::thread, 5> threads;
    for (auto& t : threads)
    {
        t = std::thread(
            [&]()
            {
                if (lock.tryReadLock())
                {
                    success_count++;
                    lock.readUnlock();
                }
                if (lock.tryWriteLock())
                {
                    success_count++;
                    lock.writeUnlock();
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    ASSERT_EQ(success_count, 0);

    lock.writeUnlock();

    for (auto& t : threads)
    {
        t = std::thread(
            [&]()
            {
                if (lock.tryReadLock())
                {
                    success_count++;
                    lock.readUnlock();
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    ASSERT_GT(success_count, 0);
}

TEST_F(ReadWriteLockTest, EdgeCase_WriteUnlockOnUnlockedLock_ShouldNotCrash)
{
    ReadWriteLock lock;

    lock.writeUnlock();
    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, EdgeCase_ReadLockAfterWriteUnlock_ShouldWork)
{
    ReadWriteLock lock;

    lock.writeLock();
    lock.writeUnlock();

    ASSERT_TRUE(lock.tryReadLock());
    lock.readUnlock();
}

TEST_F(ReadWriteLockTest, EdgeCase_WriteLockAfterReadUnlock_ShouldWork)
{
    ReadWriteLock lock;

    lock.readLock();
    lock.readUnlock();

    ASSERT_TRUE(lock.tryWriteLock());
    lock.writeUnlock();
}

TEST_F(ReadWriteLockTest, EdgeCase_ReaderCountWrap_ShouldNotHappen)
{
    ReadWriteLock lock;
    const int max_tests = 1000;

    for (int i = 0; i < max_tests; ++i)
    {
        lock.readLock();
        ASSERT_EQ(lock.readerCount(), i + 1);
    }

    for (int i = max_tests - 1; i >= 0; --i)
    {
        ASSERT_EQ(lock.readerCount(), i + 1);
        lock.readUnlock();
    }

    ASSERT_EQ(lock.readerCount(), 0);
}

TEST_F(ReadWriteLockTest, ReadLockGuard_ShouldAutoUnlock)
{
    ReadWriteLock lock;

    {
        ReadLockGuard guard(lock);
        ASSERT_TRUE(lock.isLockedForRead());
        ASSERT_EQ(lock.readerCount(), 1);
    }

    ASSERT_FALSE(lock.isLockedForRead());
    ASSERT_EQ(lock.readerCount(), 0);
}

TEST_F(ReadWriteLockTest, ReadLockGuard_MoveSemantics_ShouldTransferOwnership)
{
    ReadWriteLock lock;

    ReadLockGuard guard1(lock);
    ASSERT_TRUE(lock.isLockedForRead());

    ReadLockGuard guard2(std::move(guard1));
    ASSERT_TRUE(lock.isLockedForRead());

    ASSERT_FALSE(static_cast<bool>(guard1));
    ASSERT_TRUE(static_cast<bool>(guard2));
}

TEST_F(ReadWriteLockTest, WriteLockGuard_ShouldAutoUnlock)
{
    ReadWriteLock lock;

    {
        WriteLockGuard guard(lock);
        ASSERT_TRUE(lock.isLockedForWrite());
    }

    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, WriteLockGuard_MoveSemantics_ShouldTransferOwnership)
{
    ReadWriteLock lock;

    WriteLockGuard guard1(lock);
    ASSERT_TRUE(lock.isLockedForWrite());

    WriteLockGuard guard2(std::move(guard1));
    ASSERT_TRUE(lock.isLockedForWrite());
    ASSERT_FALSE(static_cast<bool>(guard1));
    ASSERT_TRUE(static_cast<bool>(guard2));
}

TEST_F(ReadWriteLockTest, LockGuard_ManualUnlock_ShouldWork)
{
    ReadWriteLock lock;

    WriteLockGuard guard(lock);
    ASSERT_TRUE(lock.isLockedForWrite());

    guard.unlock();
    ASSERT_FALSE(lock.isLockedForWrite());

    guard.unlock();
    ASSERT_FALSE(lock.isLockedForWrite());
}

TEST_F(ReadWriteLockTest, LockGuard_BoolOperator_ShouldIndicateLockOwnership)
{
    ReadWriteLock lock;
    ReadLockGuard guard(lock);

    ASSERT_TRUE(static_cast<bool>(guard));

    auto moved_guard = std::move(guard);
    ASSERT_FALSE(static_cast<bool>(guard));
    ASSERT_TRUE(static_cast<bool>(moved_guard));
}

TEST_F(ReadWriteLockTest, Compatibility_MultipleReadLocksFromSameThread_ShouldWork)
{
    ReadWriteLock lock;

    lock.readLock();
    lock.readLock();
    lock.readLock();

    ASSERT_EQ(lock.readerCount(), 3);

    lock.readUnlock();
    ASSERT_EQ(lock.readerCount(), 2);

    lock.readUnlock();
    ASSERT_EQ(lock.readerCount(), 1);

    lock.readUnlock();
    ASSERT_EQ(lock.readerCount(), 0);
}

TEST_F(ReadWriteLockTest, Compatibility_InterleavedReadWrite_ShouldWork)
{
    ReadWriteLock lock;

    lock.readLock();
    ASSERT_EQ(lock.readerCount(), 1);

    lock.readUnlock();
    ASSERT_EQ(lock.readerCount(), 0);

    lock.writeLock();
    ASSERT_TRUE(lock.isLockedForWrite());

    lock.writeUnlock();
    ASSERT_FALSE(lock.isLockedForWrite());

    lock.readLock();
    ASSERT_EQ(lock.readerCount(), 1);

    lock.readUnlock();
}
