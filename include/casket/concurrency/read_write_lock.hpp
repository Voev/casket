#pragma once
#include <atomic>
#include <thread>
#include <cstdint>

namespace casket
{

class ReadWriteLock final
{
private:
    mutable std::atomic<uint32_t> state_{0};

    static constexpr uint32_t WRITER_BIT = 1 << 0;
    static constexpr uint32_t READER_UNIT = 1 << 1;
    static constexpr uint32_t READER_MASK = ~WRITER_BIT;
    static constexpr uint32_t MAX_READERS = (std::numeric_limits<uint32_t>::max() >> 1);

public:
    ReadWriteLock() noexcept = default;
    ~ReadWriteLock() noexcept = default;

    ReadWriteLock(const ReadWriteLock&) = delete;
    ReadWriteLock& operator=(const ReadWriteLock&) = delete;

    ReadWriteLock(ReadWriteLock&& other) noexcept
        : state_(other.state_.load(std::memory_order_acquire))
    {
        other.state_.store(0, std::memory_order_release);
    }

    ReadWriteLock& operator=(ReadWriteLock&& other) noexcept
    {
        if (this != &other)
        {
            uint32_t expected = 0;
            while (!state_.compare_exchange_weak(expected, 0, std::memory_order_acquire, std::memory_order_relaxed))
            {
                expected = 0;
                std::this_thread::yield();
            }

            state_.store(other.state_.load(std::memory_order_acquire), std::memory_order_release);
            other.state_.store(0, std::memory_order_release);
        }
        return *this;
    }

    void readLock() noexcept
    {
        uint32_t expected = state_.load(std::memory_order_acquire);

        while (true)
        {
            const uint32_t readers = expected & READER_MASK;

            if (!(expected & WRITER_BIT) && readers < MAX_READERS)
            {
                uint32_t desired = expected + READER_UNIT;
                if (state_.compare_exchange_weak(expected, desired, std::memory_order_acquire,
                                                 std::memory_order_relaxed))
                {
                    return;
                }
            }
            else
            {
                expected = state_.load(std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }
    }

    void readUnlock() noexcept
    {
        state_.fetch_sub(READER_UNIT, std::memory_order_release);
    }

    void writeLock() noexcept
    {
        uint32_t expected = state_.load(std::memory_order_acquire);

        while (true)
        {
            if (expected == 0)
            {
                uint32_t desired = WRITER_BIT;
                if (state_.compare_exchange_weak(expected, desired, std::memory_order_acquire,
                                                 std::memory_order_relaxed))
                {
                    return;
                }
            }
            else
            {
                expected = state_.load(std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }
    }

    void writeUnlock() noexcept
    {
        state_.store(0, std::memory_order_release);
    }

    bool tryReadLock() noexcept
    {
        uint32_t expected = state_.load(std::memory_order_acquire);

        while (true)
        {
            const uint32_t readers = expected & READER_MASK;

            if ((expected & WRITER_BIT) || readers >= MAX_READERS)
            {
                return false;
            }

            uint32_t desired = expected + READER_UNIT;
            if (state_.compare_exchange_weak(expected, desired, std::memory_order_acquire, std::memory_order_relaxed))
            {
                return true;
            }
        }
    }

    bool tryWriteLock() noexcept
    {
        uint32_t expected = 0;
        return state_.compare_exchange_strong(expected, WRITER_BIT, std::memory_order_acquire,
                                              std::memory_order_relaxed);
    }

    bool isLockedForRead() const noexcept
    {
        return (state_.load(std::memory_order_acquire) & READER_MASK) != 0;
    }

    bool isLockedForWrite() const noexcept
    {
        return (state_.load(std::memory_order_acquire) & WRITER_BIT) != 0;
    }

    uint32_t readerCount() const noexcept
    {
        return (state_.load(std::memory_order_acquire) & READER_MASK) >> 1;
    }
};

class ReadLockGuard final
{
private:
    ReadWriteLock* lock_{nullptr};

public:
    explicit ReadLockGuard(ReadWriteLock& lock) noexcept
        : lock_(&lock)
    {
        lock_->readLock();
    }

    ReadLockGuard(ReadLockGuard&& other) noexcept
        : lock_(other.lock_)
    {
        other.lock_ = nullptr;
    }

    ReadLockGuard& operator=(ReadLockGuard&& other) noexcept
    {
        if (this != &other)
        {
            unlock();
            lock_ = other.lock_;
            other.lock_ = nullptr;
        }
        return *this;
    }

    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;

    ~ReadLockGuard() noexcept
    {
        unlock();
    }

    void unlock() noexcept
    {
        if (lock_)
        {
            lock_->readUnlock();
            lock_ = nullptr;
        }
    }

    explicit operator bool() const noexcept
    {
        return lock_ != nullptr;
    }
};

class WriteLockGuard final
{
private:
    ReadWriteLock* lock_{nullptr};

public:
    explicit WriteLockGuard(ReadWriteLock& lock) noexcept
        : lock_(&lock)
    {
        lock_->writeLock();
    }

    WriteLockGuard(WriteLockGuard&& other) noexcept
        : lock_(other.lock_)
    {
        other.lock_ = nullptr;
    }

    WriteLockGuard& operator=(WriteLockGuard&& other) noexcept
    {
        if (this != &other)
        {
            unlock();
            lock_ = other.lock_;
            other.lock_ = nullptr;
        }
        return *this;
    }

    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;

    ~WriteLockGuard() noexcept
    {
        unlock();
    }

    void unlock() noexcept
    {
        if (lock_)
        {
            lock_->writeUnlock();
            lock_ = nullptr;
        }
    }

    explicit operator bool() const noexcept
    {
        return lock_ != nullptr;
    }
};

} // namespace casket