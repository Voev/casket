#pragma once

#include <vector>
#include <cstring>
#include <cstddef>

namespace casket
{

class ByteBuffer
{
public:
    using value_type = uint8_t;
    using size_type = size_t;

    explicit ByteBuffer(size_t capacity)
        : data_(capacity)
        , readPos_(0)
        , writePos_(0)
    {}

    // Для записи (пакеру нужен указатель на начало свободного места)
    uint8_t* getWritePtr()
    {
        return data_.data() + writePos_;
    }

    const uint8_t* getReadPtr() const
    {
        return data_.data() + readPos_;
    }

    size_t availableRead() const
    {
        return writePos_ - readPos_;
    }

    size_t availableWrite() const
    {
        return data_.size() - writePos_;
    }

    size_t capacity() const
    {
        return data_.size();
    }

    void commitWrite(size_t bytes)
    {
        writePos_ += bytes;
    }

    void commitRead(size_t bytes)
    {
        readPos_ += bytes;
        
        // Компактификация: если прочитали много и буфер фрагментирован
        if (readPos_ > 4096 && readPos_ > availableRead() * 2)
        {
            size_t avail = availableRead();
            if (avail > 0)
            {
                std::memmove(data_.data(), getReadPtr(), avail);
            }
            readPos_ = 0;
            writePos_ = avail;
        }
        // Если всё прочитали - просто сбрасываем
        else if (readPos_ == writePos_)
        {
            readPos_ = 0;
            writePos_ = 0;
        }
    }

    void clear()
    {
        readPos_ = 0;
        writePos_ = 0;
    }

    void reset()
    {
        readPos_ = 0;
        writePos_ = 0;
    }

    void expand(size_t newCapacity)
    {
        if (newCapacity > data_.size())
        {
            data_.resize(newCapacity);
        }
    }

    // Методы для совместимости с socket_ops (если нужны)
    value_type* prepareWrite(size_type& out_available)
    {
        out_available = availableWrite();
        return getWritePtr();
    }

    const value_type* prepareRead(size_type& out_available) const
    {
        out_available = availableRead();
        return getReadPtr();
    }

    size_type writePos() const { return writePos_; }
    size_type readPos() const { return readPos_; }
    bool isEmpty() const { return availableRead() == 0; }

private:
    std::vector<uint8_t> data_;
    size_t readPos_;
    size_t writePos_;
};

} // namespace casket