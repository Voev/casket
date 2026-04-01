#pragma once
#include <cstddef>
#include <atomic>
#include <vector>

namespace casket
{

/// @brief A lock-free fixed-capacity ring buffer (circular buffer) implementation
/// @tparam T Type of elements stored in the buffer
/// @tparam Storage Storage container type with fixed size (default: std::array<T, Capacity+1>)
///
/// @details This ring buffer uses the "always keep one slot empty" strategy to distinguish
///          between full and empty states. When full, push operations fail (no overwrite).
///          The buffer is thread-safe for single producer and single consumer (SPSC).
/// @note For multiple producers or consumers, use external synchronization
template <class T, class Storage = std::vector<T>>
class LockFreeRingBuffer final
{
public:
    /// @brief Constructs a ring buffer with specified capacity
    /// @param[in] capacity Maximum number of elements the buffer can hold
    /// @throws std::invalid_argument if capacity is 0
    explicit LockFreeRingBuffer(size_t capacity)
        : storage_(capacity + 1) // +1 extra element to distinguish full/empty
        , write_(0)
        , read_(0)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("Capacity must be greater than 0");
        }
    }

    /// @brief Constructs with pre-sized storage (for std::array)
    /// @tparam U Storage type (deduced)
    /// @param[in] storage Pre-initialized storage
    explicit LockFreeRingBuffer(Storage&& storage) noexcept
        : storage_(std::move(storage))
        , write_(0)
        , read_(0)
    {
    }

    ~LockFreeRingBuffer() noexcept = default;

    // запрещаем копирование
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;

    // разрешаем перемещение
    LockFreeRingBuffer(LockFreeRingBuffer&& other) noexcept
        : storage_(std::move(other.storage_))
        , write_(other.write_.load(std::memory_order_relaxed))
        , read_(other.read_.load(std::memory_order_relaxed))
    {
    }

    LockFreeRingBuffer& operator=(LockFreeRingBuffer&& other) noexcept
    {
        if (this != &other)
        {
            storage_ = std::move(other.storage_);
            write_.store(other.write_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            read_.store(other.read_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    /// @brief Adds an element to the buffer (lock-free, SPSC)
    /// @param[in] t Element to add (will be moved if possible)
    /// @return true if the element was added, false if buffer was full
    /// @note Does NOT overwrite old elements when full
    /// @note Thread-safe for single producer
    bool push(T t) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        size_t head = write_.load(std::memory_order_relaxed);
        size_t next_head = next(head);

        // check if buffer is full
        if (next_head == read_.load(std::memory_order_acquire))
        {
            return false; // buffer full
        }

        storage_[head] = std::move(t);

        // publish the new element
        write_.store(next_head, std::memory_order_release);
        return true;
    }

    /// @brief Removes the oldest element from the buffer (lock-free, SPSC)
    /// @return true if an element was removed, false if buffer was empty
    /// @note Thread-safe for single consumer
    bool pop() noexcept
    {
        size_t tail = read_.load(std::memory_order_relaxed);

        if (tail == write_.load(std::memory_order_acquire))
        {
            return false; // buffer empty
        }

        read_.store(next(tail), std::memory_order_release);
        return true;
    }

    /// @brief Removes the oldest element from the buffer and retrieves its value
    /// @param[out] value Reference to store the removed element
    /// @return true if an element was removed, false if buffer was empty
    /// @note Thread-safe for single consumer
    bool pop(T& value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        size_t tail = read_.load(std::memory_order_relaxed);

        if (tail == write_.load(std::memory_order_acquire))
        {
            return false; // buffer empty
        }

        value = std::move(storage_[tail]);
        read_.store(next(tail), std::memory_order_release);
        return true;
    }

    /// @brief Returns the maximum number of elements the buffer can hold
    /// @return Buffer capacity
    size_t capacity() const noexcept
    {
        return storage_.size() - 1;
    }

    /// @brief Returns the current number of elements in the buffer
    /// @note This is approximate in multi-threaded context
    size_t size() const noexcept
    {
        size_t head = write_.load(std::memory_order_acquire);
        size_t tail = read_.load(std::memory_order_acquire);

        if (head >= tail)
        {
            return head - tail;
        }
        return storage_.size() - tail + head;
    }

    /// @brief Checks if the buffer is empty (approximate)
    bool empty() const noexcept
    {
        return write_.load(std::memory_order_acquire) == read_.load(std::memory_order_acquire);
    }

    /// @brief Checks if the buffer is full (approximate)
    bool full() const noexcept
    {
        return next(write_.load(std::memory_order_acquire)) == read_.load(std::memory_order_acquire);
    }

    /// @brief Returns a const reference to the oldest element
    /// @throws std::out_of_range if buffer is empty
    /// @note Not thread-safe with concurrent pop()
    const T& front() const
    {
        if (empty())
        {
            throw std::out_of_range("LockFreeRingBuffer is empty");
        }
        return storage_[read_.load(std::memory_order_acquire)];
    }

    /// @brief Returns a reference to the oldest element
    /// @throws std::out_of_range if buffer is empty
    /// @note Not thread-safe with concurrent pop()
    T& front()
    {
        if (empty())
        {
            throw std::out_of_range("LockFreeRingBuffer is empty");
        }
        return storage_[read_.load(std::memory_order_acquire)];
    }

    /// @brief Removes all elements from the buffer
    /// @note NOT thread-safe! Call only when no other threads are accessing
    void clear() noexcept
    {
        write_.store(0, std::memory_order_relaxed);
        read_.store(0, std::memory_order_relaxed);
    }

    /// @brief Returns the next index (with wrap-around)
    inline size_t next(size_t idx) const noexcept
    {
        return (idx + 1) % storage_.size();
    }

private:
    Storage storage_;           ///< Underlying storage container
    std::atomic<size_t> write_; ///< Write index (next position to write to)
    std::atomic<size_t> read_;  ///< Read index (next position to read from)
};

} // namespace casket