#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace casket::lf
{

template <typename T, size_t Capacity>
class RingBuffer
{
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "Capacity must be positive power of two");

public:
    RingBuffer()
        : writeIndex_(0)
        , readIndex_(0)
    {
        for (size_t i = 0; i < Capacity; ++i)
        {
            new (&buffer_[i]) T();
        }
    }

    ~RingBuffer() noexcept
    {
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    bool try_push(const T& value)
    {
        size_t writeIndex = writeIndex_.load(std::memory_order_acquire);
        size_t readIndex = readIndex_.load(std::memory_order_acquire);

        if (writeIndex - readIndex >= Capacity)
        {
            return false;
        }

        size_t pos = writeIndex & (Capacity - 1);
        buffer_[pos] = value;

        writeIndex_.store(writeIndex + 1, std::memory_order_release);
        return true;
    }

    bool try_push(T&& value)
    {
        size_t writeIndex = writeIndex_.load(std::memory_order_acquire);
        size_t readIndex = readIndex_.load(std::memory_order_acquire);

        if (writeIndex - readIndex >= Capacity)
        {
            return false;
        }

        size_t pos = writeIndex & (Capacity - 1);
        buffer_[pos] = std::move(value);

        writeIndex_.store(writeIndex + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& value)
    {
        size_t readIndex = readIndex_.load(std::memory_order_acquire);
        size_t writeIndex = writeIndex_.load(std::memory_order_acquire);

        if (readIndex >= writeIndex)
        {
            return false;
        }

        size_t pos = readIndex & (Capacity - 1);
        value = std::move(buffer_[pos]);

        readIndex_.store(readIndex + 1, std::memory_order_release);
        return true;
    }

    size_t try_push_batch(const T* values, size_t count)
    {
        if (count == 0)
            return 0;

        size_t writeIndex = writeIndex_.load(std::memory_order_acquire);

        while (true)
        {
            size_t readIndex = readIndex_.load(std::memory_order_acquire);
            size_t available = Capacity - (writeIndex - readIndex);

            if (available == 0)
            {
                return 0;
            }

            size_t toPush = std::min(count, available);
            size_t newWriteIndex = writeIndex + toPush;

            if (writeIndex_.compare_exchange_weak(writeIndex, newWriteIndex, std::memory_order_acq_rel,
                                                   std::memory_order_acquire))
            {

                for (size_t i = 0; i < toPush; ++i)
                {
                    size_t pos = (writeIndex + i) & (Capacity - 1);
                    buffer_[pos] = std::move(const_cast<T&>(values[i]));
                }

                return toPush;
            }
        }
    }

    size_t try_pop_batch(T* out, size_t maxCount)
    {
        if (maxCount == 0)
            return 0;

        size_t readIndex = readIndex_.load(std::memory_order_acquire);
        size_t writeIndex = writeIndex_.load(std::memory_order_acquire);

        size_t available = writeIndex - readIndex;
        if (available == 0)
        {
            return 0;
        }

        size_t toPop = std::min(maxCount, available);
        size_t newReadIndex = readIndex + toPop;

        for (size_t i = 0; i < toPop; ++i)
        {
            size_t pos = (readIndex + i) & (Capacity - 1);
            out[i] = std::move(buffer_[pos]);
        }

        readIndex_.store(newReadIndex, std::memory_order_release);
        return toPop;
    }

    std::vector<T> try_pop_batch(size_t maxCount)
    {
        std::vector<T> result;
        result.reserve(maxCount);

        size_t readIndex = readIndex_.load(std::memory_order_acquire);
        size_t writeIndex = writeIndex_.load(std::memory_order_acquire);

        size_t available = writeIndex - readIndex;
        if (available == 0)
        {
            return result;
        }

        size_t toPop = std::min(maxCount, available);

        for (size_t i = 0; i < toPop; ++i)
        {
            size_t pos = (readIndex + i) & (Capacity - 1);
            result.push_back(std::move(buffer_[pos]));
        }

        readIndex_.store(readIndex + toPop, std::memory_order_release);

        return result;
    }

    size_t size() const
    {
        return writeIndex_.load(std::memory_order_acquire) - readIndex_.load(std::memory_order_acquire);
    }

    bool empty() const
    {
        return size() == 0;
    }
    bool full() const
    {
        return size() >= Capacity;
    }
    constexpr size_t capacity() const
    {
        return Capacity;
    }

    void clear()
    {
        size_t readIndex = readIndex_.load(std::memory_order_acquire);
        size_t writeIndex = writeIndex_.load(std::memory_order_acquire);
        readIndex_.store(writeIndex, std::memory_order_release);
    }

private:
    alignas(64) std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> writeIndex_;
    alignas(64) std::atomic<size_t> readIndex_;
};

} // namespace casket::lf