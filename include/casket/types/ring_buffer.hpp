#pragma once
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace casket
{

/// @brief A fixed-capacity ring buffer (circular buffer) implementation
/// @tparam T Type of elements stored in the buffer
/// @tparam Storage Storage container type (default: std::vector<T>)
///
/// @details This ring buffer uses the "always keep one slot empty" strategy to distinguish
///          between full and empty states. When full, new elements overwrite the oldest ones.
///          The buffer is not thread-safe.
template <class T, class Storage = std::vector<T>>
class RingBuffer final
{
public:
    /// @brief Constructs a ring buffer with specified capacity
    /// @param[in] capacity Maximum number of elements the buffer can hold
    /// @throws std::invalid_argument if capacity is 0
    explicit RingBuffer(size_t capacity)
        : storage_(capacity + 1) // +1 extra element to distinguish full/empty
        , write_(0)
        , read_(0)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("Capacity must be greater than 0");
        }
    }

    /// @brief Destructor
    ~RingBuffer() noexcept = default;

    /// @brief Copy constructor (deleted)
    RingBuffer(const RingBuffer&) = delete;

    /// @brief Copy assignment operator (deleted)
    RingBuffer& operator=(const RingBuffer&) = delete;

    /// @brief Adds an element to the buffer
    /// @param[in] t Element to add (will be moved if possible)
    /// @return true if the buffer had space before adding, false if buffer was full
    /// @note When buffer is full, the oldest element is overwritten
    /// @note noexcept if T's move assignment is noexcept
    bool push(T t) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        bool had_space = !full();

        storage_[write_] = std::move(t);
        write_ = next(write_);

        if (write_ == read_)
        {
            read_ = next(read_);
        }

        return had_space;
    }

    /// @brief Removes the oldest element from the buffer
    /// @return true if an element was removed, false if buffer was empty
    bool pop() noexcept
    {
        if (empty())
        {
            return false;
        }
        read_ = next(read_);
        return true;
    }

    /// @brief Removes the oldest element from the buffer and retrieves its value
    /// @param[out] value Reference to store the removed element
    /// @return true if an element was removed, false if buffer was empty
    /// @note noexcept if T's move construction is noexcept
    bool pop(T& value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        if (empty())
        {
            return false;
        }
        value = std::move(storage_[read_]);
        read_ = next(read_);
        return true;
    }

    /// @brief Returns the maximum number of elements the buffer can hold
    /// @return Buffer capacity
    size_t capacity() const noexcept
    {
        return storage_.size() - 1;
    }

    /// @brief Returns the current number of elements in the buffer
    /// @return Current buffer size
    size_t size() const noexcept
    {
        if (write_ >= read_)
        {
            return write_ - read_;
        }
        return storage_.size() - read_ + write_;
    }

    /// @brief Checks if the buffer is empty
    /// @return true if buffer contains no elements
    bool empty() const noexcept
    {
        return write_ == read_;
    }

    /// @brief Checks if the buffer is full
    /// @return true if buffer is at maximum capacity
    bool full() const noexcept
    {
        return next(write_) == read_;
    }

    /// @brief Returns a const reference to the oldest element
    /// @return Const reference to the front element
    /// @throws std::out_of_range if buffer is empty
    const T& front() const
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        return storage_[read_];
    }

    /// @brief Returns a reference to the oldest element
    /// @return Reference to the front element
    /// @throws std::out_of_range if buffer is empty
    T& front()
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        return storage_[read_];
    }

    /// @brief Returns a const reference to the newest element
    /// @return Const reference to the back element
    /// @throws std::out_of_range if buffer is empty
    const T& back() const
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        size_t back_idx = (write_ == 0) ? storage_.size() - 1 : write_ - 1;
        return storage_[back_idx];
    }

    /// @brief Returns a reference to the newest element
    /// @return Reference to the back element
    /// @throws std::out_of_range if buffer is empty
    T& back()
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        size_t back_idx = (write_ == 0) ? storage_.size() - 1 : write_ - 1;
        return storage_[back_idx];
    }

    /// @brief Removes all elements from the buffer
    /// @note This operation is O(1) and does not call destructors of stored elements
    void clear() noexcept
    {
        write_ = 0;
        read_ = 0;
    }

private:
    /// @brief Advances an index to the next position (with wrap-around)
    /// @param[in] idx Current index
    /// @return Next index in the circular buffer
    inline size_t next(size_t idx) const noexcept
    {
        return (idx + 1) % storage_.size();
    }

private:
    Storage storage_; ///< Underlying storage container
    size_t write_;    ///< Write index (next position to write to)
    size_t read_;     ///< Read index (next position to read from)
};

} // namespace casket