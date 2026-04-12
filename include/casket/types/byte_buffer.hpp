#pragma once

#include <memory>
#include <cstring>
#include <cstdint>

namespace casket
{

class ByteBuffer
{
public:
    static constexpr size_t DEFAULT_SIZE = 8192;

    explicit ByteBuffer(size_t size = DEFAULT_SIZE)
        : data_(std::make_unique<uint8_t[]>(size))
        , size_(size)
    {
    }

    uint8_t* data()
    {
        return data_.get();
    }
    const uint8_t* data() const
    {
        return data_.get();
    }
    size_t size() const
    {
        return size_;
    }

    size_t writePos() const
    {
        return writePos_;
    }
    size_t readPos() const
    {
        return readPos_;
    }

    size_t writeIndex() const
    {
        return writePos_ % size_;
    }
    size_t readIndex() const
    {
        return readPos_ % size_;
    }

    size_t available() const
    {
        return size_ - (writePos_ - readPos_);
    }

    size_t readable() const
    {
        return writePos_ - readPos_;
    }

    void clear()
    {
        writePos_ = 0;
        readPos_ = 0;
    }

    void reset()
    {
        clear();
    }

    size_t write(const uint8_t* src, size_t len)
    {
        size_t written = 0;

        while (written < len && available() > 0)
        {
            size_t idx = writeIndex();
            size_t spaceToEnd = size_ - idx;
            size_t toWrite = std::min(len - written, spaceToEnd);

            size_t maxWrite = size_ - readable();
            if (toWrite > maxWrite)
            {
                toWrite = maxWrite;
            }

            if (toWrite == 0)
                break;

            memcpy(data_.get() + idx, src + written, toWrite);
            writePos_ += toWrite;
            written += toWrite;
        }

        return written;
    }

    size_t read(uint8_t* dst, size_t len)
    {
        size_t read = 0;

        while (read < len && readable() > 0)
        {
            size_t idx = readIndex();
            size_t dataToEnd = size_ - idx;
            size_t toRead = std::min(len - read, dataToEnd);

            if (toRead > readable())
            {
                toRead = readable();
            }

            if (toRead == 0)
                break;

            memcpy(dst + read, data_.get() + idx, toRead);
            readPos_ += toRead;
            read += toRead;
        }

        return read;
    }

    uint8_t* prepareWrite(size_t& available_out)
    {
        size_t idx = writeIndex();
        size_t spaceToEnd = size_ - idx;
        size_t maxWrite = size_ - readable();

        available_out = std::min(spaceToEnd, maxWrite);
        if (available_out == 0)
            return nullptr;

        return data_.get() + idx;
    }

    void commit(size_t bytes)
    {
        writePos_ += bytes;
    }

    uint8_t* prepareRead(size_t& available_out)
    {
        size_t idx = readIndex();
        size_t dataToEnd = size_ - idx;

        available_out = std::min(dataToEnd, readable());
        if (available_out == 0)
            return nullptr;

        return data_.get() + idx;
    }

    void consume(size_t bytes)
    {
        readPos_ += bytes;
    }

    bool skip(size_t bytes)
    {
        if (readable() < bytes)
            return false;
        readPos_ += bytes;
        return true;
    }

    uint8_t* peek(size_t& available_out)
    {
        return prepareRead(available_out);
    }

    size_t linearRead(uint8_t*& out_ptr)
    {
        size_t idx = readIndex();
        size_t dataToEnd = size_ - idx;
        size_t avail = std::min(dataToEnd, readable());
        out_ptr = data_.get() + idx;
        return avail;
    }

    size_t linearWrite(uint8_t*& out_ptr)
    {
        size_t idx = writeIndex();
        size_t spaceToEnd = size_ - idx;
        size_t maxWrite = size_ - readable();
        size_t avail = std::min(spaceToEnd, maxWrite);
        out_ptr = data_.get() + idx;
        return avail;
    }

private:
    std::unique_ptr<uint8_t[]> data_;
    size_t size_{0};
    size_t writePos_{0};
    size_t readPos_{0};
};

} // namespace casket