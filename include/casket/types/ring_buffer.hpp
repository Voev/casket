#pragma once
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace casket
{

template <class T, class Storage = std::vector<T>>
class RingBuffer final
{
public:
    explicit RingBuffer(size_t capacity)
        : storage_(capacity + 1) // +1 лишний элемент для различения пусто/полно
        , write_(0)
        , read_(0)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("Capacity must be greater than 0");
        }
    }

    ~RingBuffer() noexcept = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

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

    bool pop() noexcept
    {
        if (empty())
        {
            return false;
        }
        read_ = next(read_);
        return true;
    }

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

    size_t capacity() const noexcept
    {
        return storage_.size() - 1;
    }

    size_t size() const noexcept
    {
        if (write_ >= read_)
        {
            return write_ - read_;
        }
        return storage_.size() - read_ + write_;
    }

    bool empty() const noexcept
    {
        return write_ == read_;
    }

    bool full() const noexcept
    {
        return next(write_) == read_;
    }

    const T& front() const
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        return storage_[read_];
    }

    T& front()
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        return storage_[read_];
    }

    const T& back() const
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        size_t back_idx = (write_ == 0) ? storage_.size() - 1 : write_ - 1;
        return storage_[back_idx];
    }

    T& back()
    {
        if (empty())
        {
            throw std::out_of_range("RingBuffer is empty");
        }
        size_t back_idx = (write_ == 0) ? storage_.size() - 1 : write_ - 1;
        return storage_[back_idx];
    }

    void clear() noexcept
    {
        write_ = 0;
        read_ = 0;
    }

private:
    inline size_t next(size_t idx) const noexcept
    {
        return (idx + 1) % storage_.size();
    }

private:
    Storage storage_;
    size_t write_;
    size_t read_;
};

} // namespace casket