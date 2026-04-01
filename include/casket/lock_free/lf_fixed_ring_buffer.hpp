#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace casket
{

/// @brief Lock-free ring buffer with fixed capacity (compile-time)
/// @tparam T Type of elements
/// @tparam Capacity Maximum number of elements (must be power of two for optimization)
template <typename T, size_t Capacity>
class LockFreeFixedRingBuffer final
{
    static_assert(Capacity > 0, "Capacity must be greater than 0");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity should be power of two for optimal performance");

private:
    static constexpr size_t BUFFER_SIZE = Capacity + 1;
    static constexpr size_t MASK = BUFFER_SIZE - 1;

    std::array<T, BUFFER_SIZE> storage_;
    std::atomic<size_t> write_{0};
    std::atomic<size_t> read_{0};

    inline size_t next(size_t idx) const noexcept
    {
        return (idx + 1) & MASK; // faster than modulo for power of two
    }

public:
    LockFreeFixedRingBuffer() = default;

    bool push(T t) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        size_t head = write_.load(std::memory_order_relaxed);
        size_t next_head = next(head);

        if (next_head == read_.load(std::memory_order_acquire))
        {
            return false;
        }

        storage_[head] = std::move(t);
        write_.store(next_head, std::memory_order_release);
        return true;
    }

    bool pop(T& value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        size_t tail = read_.load(std::memory_order_relaxed);

        if (tail == write_.load(std::memory_order_acquire))
        {
            return false;
        }

        value = std::move(storage_[tail]);
        read_.store(next(tail), std::memory_order_release);
        return true;
    }

    bool pop() noexcept
    {
        size_t tail = read_.load(std::memory_order_relaxed);

        if (tail == write_.load(std::memory_order_acquire))
        {
            return false;
        }

        read_.store(next(tail), std::memory_order_release);
        return true;
    }

    size_t size() const noexcept
    {
        size_t head = write_.load(std::memory_order_acquire);
        size_t tail = read_.load(std::memory_order_acquire);
        return (head >= tail) ? (head - tail) : (BUFFER_SIZE - tail + head);
    }

    size_t capacity() const noexcept
    {
        return Capacity;
    }

    bool empty() const noexcept
    {
        return write_.load(std::memory_order_acquire) == read_.load(std::memory_order_acquire);
    }

    bool full() const noexcept
    {
        return next(write_.load(std::memory_order_acquire)) == read_.load(std::memory_order_acquire);
    }

    void clear() noexcept
    {
        write_.store(0, std::memory_order_relaxed);
        read_.store(0, std::memory_order_relaxed);
    }
};

} // namespace casket