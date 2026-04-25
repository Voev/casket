#pragma once

#include <vector>
#include <cstring>
#include <cstddef>

namespace casket
{

class ByteBuffer final
{
public:
    using value_type = uint8_t;
    using size_type = size_t;

    ~ByteBuffer() = default;

    explicit ByteBuffer(size_type capacity)
        : data_(capacity)
        , readPos_(0)
        , writePos_(0)
    {
    }

    ByteBuffer(const ByteBuffer& other)
        : data_(other.data_)
        , readPos_(other.readPos_)
        , writePos_(other.writePos_)
    {
    }

    ByteBuffer(ByteBuffer&& other) noexcept
        : data_(std::move(other.data_))
        , readPos_(other.readPos_)
        , writePos_(other.writePos_)
    {
        other.readPos_ = 0;
        other.writePos_ = 0;
    }

    ByteBuffer& operator=(const ByteBuffer& other)
    {
        if (this != &other)
        {
            data_ = other.data_;
            readPos_ = other.readPos_;
            writePos_ = other.writePos_;
        }
        return *this;
    }

    ByteBuffer& operator=(ByteBuffer&& other) noexcept
    {
        if (this != &other)
        {
            data_ = std::move(other.data_);
            readPos_ = other.readPos_;
            writePos_ = other.writePos_;
            other.readPos_ = 0;
            other.writePos_ = 0;
        }
        return *this;
    }

    value_type* getWritePtr() noexcept
    {
        return data_.data() + writePos_;
    }

    const value_type* getReadPtr() const noexcept
    {
        return data_.data() + readPos_;
    }

    size_type availableRead() const noexcept
    {
        return writePos_ - readPos_;
    }

    size_type availableWrite() const noexcept
    {
        return data_.size() - writePos_;
    }

    size_type capacity() const noexcept
    {
        return data_.size();
    }

    void commitWrite(size_type bytes) noexcept
    {
        writePos_ += bytes;
    }

    void commitRead(size_type bytes) noexcept
    {
        readPos_ += bytes;

        if (readPos_ > capacity() / 2)
        {
            size_t avail = availableRead();
            if (avail > 0)
            {
                std::memmove(data_.data(), getReadPtr(), avail);
            }
            readPos_ = 0;
            writePos_ = avail;
        }
        else if (readPos_ == writePos_)
        {
            readPos_ = 0;
            writePos_ = 0;
        }
    }

    void clear() noexcept
    {
        readPos_ = 0;
        writePos_ = 0;
    }

    void reset() noexcept
    {
        readPos_ = 0;
        writePos_ = 0;
    }

    void expand(size_type newCapacity)
    {
        if (newCapacity > data_.size())
        {
            data_.resize(newCapacity);
        }
    }

    value_type* prepareWrite(size_type& out)
    {
        out = availableWrite();
        return getWritePtr();
    }

    const value_type* prepareRead(size_type& out) const noexcept
    {
        out = availableRead();
        return getReadPtr();
    }

    size_type writePos() const noexcept
    {
        return writePos_;
    }
    size_type readPos() const noexcept
    {
        return readPos_;
    }
    bool isEmpty() const noexcept
    {
        return availableRead() == 0;
    }

private:
    std::vector<value_type> data_;
    size_t readPos_;
    size_t writePos_;
};

} // namespace casket