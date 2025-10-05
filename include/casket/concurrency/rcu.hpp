
#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <thread>

namespace casket
{

class RCU final
{
public:
    using Epoch = uint64_t;

    RCU() noexcept
        : globalEpoch_(0)
    {
        readerCounters_[0].store(0, std::memory_order_relaxed);
        readerCounters_[1].store(0, std::memory_order_relaxed);
    }

    ~RCU() noexcept
    {
    }

    Epoch readLock() noexcept
    {
        Epoch epoch;
        int counterIndex;

        while (true)
        {
            epoch = globalEpoch_.load(std::memory_order_acquire);
            counterIndex = static_cast<int>(epoch & 1);

            readerCounters_[counterIndex].fetch_add(1, std::memory_order_acquire);

            if (globalEpoch_.load(std::memory_order_acquire) == epoch)
            {
                return epoch;
            }

            readerCounters_[counterIndex].fetch_sub(1, std::memory_order_release);
        }
    }

    void readUnlock(Epoch epoch) noexcept
    {
        int counterIndex = static_cast<int>(epoch & 1);
        readerCounters_[counterIndex].fetch_sub(1, std::memory_order_release);
    }

    void synchronize() noexcept
    {
        auto oldEpoch = globalEpoch_.load(std::memory_order_acquire);
        auto newEpoch = oldEpoch + 1;

        globalEpoch_.store(newEpoch, std::memory_order_release);

        waitForReaders(static_cast<int>(oldEpoch & 1));
    }

    uint64_t getEpoch() const noexcept
    {
        return globalEpoch_.load(std::memory_order_relaxed);
    }

private:
    void waitForReaders(int epoch_index) noexcept
    {
        while (true)
        {
            int count = readerCounters_[epoch_index].load(std::memory_order_acquire);
            if (count == 0)
            {
                break;
            }
            std::this_thread::yield();
        }
    }

    alignas(64) std::atomic<uint64_t> globalEpoch_;
    alignas(64) std::atomic<int32_t> readerCounters_[2];
};

template <typename T>
class RCUReadHandle final
{
public:
    explicit RCUReadHandle(T* data, RCU& rcu)
        : data_(data)
        , rcu_(rcu)
        , epoch_(rcu_.readLock())
    {
    }

    ~RCUReadHandle()
    {
        if (data_ != nullptr)
        {
            rcu_.readUnlock(epoch_);
        }
    }

    RCUReadHandle(const RCUReadHandle&) = delete;
    RCUReadHandle& operator=(const RCUReadHandle&) = delete;

    RCUReadHandle(RCUReadHandle&& other) noexcept
        : data_(std::exchange(other.data_, nullptr))
        , rcu_(other.rcu_)
        , epoch_(other.epoch_)
    {
    }

    RCUReadHandle& operator=(RCUReadHandle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            data_ = std::exchange(other.data_, nullptr);
            rcu_ = other.rcu_;
            epoch_ = other.epoch_;
        }
        return *this;
    }

    T* get() const noexcept
    {
        return data_;
    }

    T* operator->() const noexcept
    {
        return data_;
    }

    T& operator*() const noexcept
    {
        return *data_;
    }

    explicit operator bool() const noexcept
    {
        return data_ != nullptr;
    }

    void reset() noexcept
    {
        if (data_ != nullptr)
        {
            rcu_.readUnlock(epoch_);
            data_ = nullptr;
        }
    }

private:
    T* data_;
    RCU& rcu_;
    RCU::Epoch epoch_;
};

} // namespace casket
