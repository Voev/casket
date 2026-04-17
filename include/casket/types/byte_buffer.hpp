#pragma once

#include <atomic>
#include <cstring>
#include <string_view>
#include <array>
#include <algorithm>
#include <vector>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

namespace casket
{

class ByteBuffer
{
public:
    using value_type = char;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

private:
    size_t capacity_{0};
    std::vector<char> buffer_{};
    std::atomic<size_type> readPos_{0};
    std::atomic<size_type> writePos_{0};

    size_type wrap(size_type pos) const noexcept
    {
        return pos & (capacity_ - 1);
    }

    std::pair<char*, size_type> getWriteChunk() noexcept
    {
        size_type write = writePos_.load(std::memory_order_acquire);
        size_type read = readPos_.load(std::memory_order_acquire);

        size_type used = write - read;
        size_type free = capacity_ - used;

        if (free == 0)
        {
            return {nullptr, 0};
        }

        size_type writeIdx = wrap(write);
        size_type contiguous = capacity_ - writeIdx;

        if (contiguous > free)
        {
            contiguous = free;
        }

        return {buffer_.data() + writeIdx, contiguous};
    }

    std::pair<const char*, size_type> getReadChunk() const noexcept
    {
        size_type read = readPos_.load(std::memory_order_acquire);
        size_type write = writePos_.load(std::memory_order_acquire);

        size_type available = write - read;
        if (available == 0)
        {
            return {nullptr, 0};
        }

        size_type readIdx = wrap(read);
        size_type contiguous = capacity_ - readIdx;

        if (contiguous > available)
        {
            contiguous = available;
        }

        return {buffer_.data() + readIdx, contiguous};
    }

public:
    explicit ByteBuffer(size_t capacity)
        : capacity_(capacity)
        , buffer_(capacity)
    {
        if ((capacity & (capacity - 1)) != 0)
        {
            size_t newCapacity = 1;
            while (newCapacity < capacity) newCapacity <<= 1;
            capacity_ = newCapacity;
            buffer_.resize(capacity_);
        }
    }

    ByteBuffer(const ByteBuffer&) = delete;
    ByteBuffer& operator=(const ByteBuffer&) = delete;

    ByteBuffer(ByteBuffer&& other) noexcept
        : capacity_(other.capacity_)
        , buffer_(std::move(other.buffer_))
        , readPos_(other.readPos_.load())
        , writePos_(other.writePos_.load())
    {
        other.clear();
        other.capacity_ = 0;
    }

    ByteBuffer& operator=(ByteBuffer&& other) noexcept
    {
        if (this != &other)
        {
            capacity_ = other.capacity_;
            buffer_ = std::move(other.buffer_);
            readPos_.store(other.readPos_.load());
            writePos_.store(other.writePos_.load());
            other.clear();
            other.capacity_ = 0;
        }
        return *this;
    }

    size_type write(const void* data, size_type len) noexcept
    {
        if (len == 0)
            return 0;

        const char* src = static_cast<const char*>(data);
        size_type written = 0;

        while (written < len)
        {
            auto [dst, avail] = getWriteChunk();
            if (avail == 0)
                break;

            size_type toWrite = std::min(len - written, avail);
            std::memcpy(dst, src + written, toWrite);
            writePos_.fetch_add(toWrite, std::memory_order_release);
            written += toWrite;
        }

        return written;
    }

    size_type read(void* data, size_type len) noexcept
    {
        if (len == 0)
            return 0;

        char* dst = static_cast<char*>(data);
        size_type readBytes = 0;

        while (readBytes < len)
        {
            auto [src, avail] = getReadChunk();
            if (avail == 0)
                break;

            size_type toRead = std::min(len - readBytes, avail);
            std::memcpy(dst + readBytes, src, toRead);
            readPos_.fetch_add(toRead, std::memory_order_release);
            readBytes += toRead;
        }

        return readBytes;
    }

    size_type consume(size_type len) noexcept
    {
        size_type available = availableRead();
        size_type toConsume = std::min(len, available);

        if (toConsume > 0)
        {
            readPos_.fetch_add(toConsume, std::memory_order_release);
        }

        return toConsume;
    }

    std::string_view peekContiguous(size_type maxLen = 0) const noexcept
    {
        if (maxLen == 0) maxLen = capacity_;
        
        auto [data, len] = getReadChunk();
        if (len == 0)
            return {};

        size_type viewLen = std::min(len, maxLen);
        return std::string_view(data, viewLen);
    }

    std::string_view peek(size_type len)
    {
        if (len == 0)
            return {};

        auto [first, firstLen] = getReadChunk();
        if (firstLen >= len)
        {
            return std::string_view(first, len);
        }

        static thread_local std::vector<char> temp;
        if (temp.size() < len)
        {
            temp.resize(len);
        }

        std::memcpy(temp.data(), first, firstLen);

        size_type secondLen = len - firstLen;
        size_type secondStart = wrap(readPos_.load() + firstLen);
        std::memcpy(temp.data() + firstLen, buffer_.data() + secondStart, secondLen);

        return std::string_view(temp.data(), len);
    }

    bool peekCopy(void* data, size_type len) const noexcept
    {
        if (availableRead() < len)
            return false;

        char* dst = static_cast<char*>(data);
        size_type read = readPos_.load(std::memory_order_acquire);
        size_type write = writePos_.load(std::memory_order_acquire);
        size_type available = write - read;

        if (available < len)
            return false;

        size_type readIdx = wrap(read);
        size_type firstChunk = std::min(capacity_ - readIdx, len);

        std::memcpy(dst, buffer_.data() + readIdx, firstChunk);

        if (firstChunk < len)
        {
            std::memcpy(dst + firstChunk, buffer_.data(), len - firstChunk);
        }

        return true;
    }

    bool readExact(void* data, size_type len) noexcept
    {
        if (availableRead() < len)
            return false;
        return read(data, len) == len;
    }

    bool writeExact(const void* data, size_type len) noexcept
    {
        if (availableWrite() < len)
            return false;
        return write(data, len) == len;
    }

    size_type findChar(char delimiter) const noexcept
    {
        size_type read = readPos_.load(std::memory_order_acquire);
        size_type write = writePos_.load(std::memory_order_acquire);
        size_type available = write - read;

        for (size_type i = 0; i < available; ++i)
        {
            size_type pos = wrap(read + i);
            if (buffer_[pos] == delimiter)
            {
                return i + 1;
            }
        }

        return 0;
    }

    size_type findPattern(const void* pattern, size_type patternLen) const noexcept
    {
        if (patternLen == 0 || patternLen > availableRead())
            return 0;

        const char* pat = static_cast<const char*>(pattern);
        size_type read = readPos_.load(std::memory_order_acquire);
        size_type write = writePos_.load(std::memory_order_acquire);
        size_type available = write - read;

        for (size_type i = 0; i <= available - patternLen; ++i)
        {
            bool found = true;
            for (size_type j = 0; j < patternLen; ++j)
            {
                size_type pos = wrap(read + i + j);
                if (buffer_[pos] != pat[j])
                {
                    found = false;
                    break;
                }
            }
            if (found)
                return i;
        }

        return 0;
    }

    size_type findAnyOf(const char* delimiters, size_type delimLen) const noexcept
    {
        size_type read = readPos_.load(std::memory_order_acquire);
        size_type write = writePos_.load(std::memory_order_acquire);
        size_type available = write - read;

        for (size_type i = 0; i < available; ++i)
        {
            size_type pos = wrap(read + i);
            char c = buffer_[pos];

            for (size_type j = 0; j < delimLen; ++j)
            {
                if (c == delimiters[j])
                {
                    return i + 1;
                }
            }
        }

        return 0;
    }

    size_type getWriteIovec(struct iovec* iov, size_type maxIov) noexcept
    {
        if (maxIov == 0)
            return 0;

        size_type write = writePos_.load(std::memory_order_acquire);
        size_type read = readPos_.load(std::memory_order_acquire);

        size_type used = write - read;
        size_type free = capacity_ - used;

        if (free == 0)
            return 0;

        size_type writeIdx = wrap(write);
        size_type firstChunk = capacity_ - writeIdx;

        iov[0].iov_base = buffer_.data() + writeIdx;
        iov[0].iov_len = std::min(firstChunk, free);

        if (maxIov > 1 && iov[0].iov_len < free)
        {
            iov[1].iov_base = buffer_.data();
            iov[1].iov_len = free - iov[0].iov_len;
            return 2;
        }

        return 1;
    }

    size_type getReadIovec(struct iovec* iov, size_type maxIov) const noexcept
    {
        if (maxIov == 0)
            return 0;

        size_type read = readPos_.load(std::memory_order_acquire);
        size_type write = writePos_.load(std::memory_order_acquire);

        size_type available = write - read;
        if (available == 0)
            return 0;

        size_type readIdx = wrap(read);
        size_type firstChunk = capacity_ - readIdx;

        iov[0].iov_base = const_cast<char*>(buffer_.data() + readIdx);
        iov[0].iov_len = std::min(firstChunk, available);

        if (maxIov > 1 && iov[0].iov_len < available)
        {
            iov[1].iov_base = const_cast<char*>(buffer_.data());
            iov[1].iov_len = available - iov[0].iov_len;
            return 2;
        }

        return 1;
    }

    void commitWrite(size_type bytesWritten) noexcept
    {
        if (bytesWritten > 0)
        {
            writePos_.fetch_add(bytesWritten, std::memory_order_release);
        }
    }

    void commitRead(size_type bytesRead) noexcept
    {
        if (bytesRead > 0)
        {
            readPos_.fetch_add(bytesRead, std::memory_order_release);
        }
    }

    ssize_t recvFromSocket(int fd, int flags = 0) noexcept
    {
        struct iovec iov[2];
        size_t iovcnt = getWriteIovec(iov, 2);

        if (iovcnt == 0)
        {
            errno = ENOBUFS;
            return -1;
        }

        struct msghdr msg{};
        msg.msg_iov = iov;
        msg.msg_iovlen = iovcnt;

        ssize_t n = recvmsg(fd, &msg, flags);
        if (n > 0)
        {
            commitWrite(n);
        }

        return n;
    }

    ssize_t sendToSocket(int fd, int flags = 0) noexcept
    {
        struct iovec iov[2];
        size_t iovcnt = getReadIovec(iov, 2);

        if (iovcnt == 0)
            return 0;

        struct msghdr msg{};
        msg.msg_iov = iov;
        msg.msg_iovlen = iovcnt;

        ssize_t n = sendmsg(fd, &msg, flags | MSG_NOSIGNAL);
        if (n > 0)
        {
            commitRead(n);
        }

        return n;
    }

    size_type capacity() const noexcept
    {
        return capacity_;
    }

    size_type availableRead() const noexcept
    {
        return writePos_.load(std::memory_order_acquire) - readPos_.load(std::memory_order_acquire);
    }

    size_type availableWrite() const noexcept
    {
        return capacity_ - availableRead();
    }

    bool isEmpty() const noexcept
    {
        return availableRead() == 0;
    }

    bool isFull() const noexcept
    {
        return availableWrite() == 0;
    }

    float utilization() const noexcept
    {
        return static_cast<float>(availableRead()) / capacity_;
    }

    void clear() noexcept
    {
        readPos_.store(0, std::memory_order_release);
        writePos_.store(0, std::memory_order_release);
    }

    void defragment() noexcept
    {
        size_type read = readPos_.load(std::memory_order_acquire);
        if (read == 0)
            return;

        size_type write = writePos_.load(std::memory_order_acquire);
        size_type used = write - read;

        if (used == 0)
        {
            clear();
            return;
        }

        if (wrap(read) + used <= capacity_)
        {
            std::memmove(buffer_.data(), buffer_.data() + wrap(read), used);
        }
        else
        {
            size_type firstPart = capacity_ - wrap(read);
            std::vector<char> temp(used);
            
            std::memcpy(temp.data(), buffer_.data() + wrap(read), firstPart);
            std::memcpy(temp.data() + firstPart, buffer_.data(), used - firstPart);
            std::memcpy(buffer_.data(), temp.data(), used);
        }

        readPos_.store(0, std::memory_order_release);
        writePos_.store(used, std::memory_order_release);
    }

    bool reserveFront(size_type len) noexcept
    {
        size_type read = readPos_.load(std::memory_order_acquire);
        if (len > read)
            return false;

        readPos_.store(read - len, std::memory_order_release);
        return true;
    }

    void truncate(size_type newSize) noexcept
    {
        size_type current = availableRead();
        if (newSize < current)
        {
            readPos_.store(readPos_.load() + (current - newSize), std::memory_order_release);
        }
    }

    char* data() noexcept
    {
        return buffer_.data();
    }
    
    const char* data() const noexcept
    {
        return buffer_.data();
    }

    char& operator[](size_type pos) noexcept
    {
        return buffer_[wrap(pos)];
    }

    const char& operator[](size_type pos) const noexcept
    {
        return buffer_[wrap(pos)];
    }

    size_type readPos() const noexcept
    {
        return readPos_.load();
    }
    
    size_type writePos() const noexcept
    {
        return writePos_.load();
    }

    std::string toString() const
    {
        std::string result;
        result.reserve(availableRead());

        auto [data, len] = getReadChunk();
        result.append(data, len);

        if (len < availableRead())
        {
            size_type remaining = availableRead() - len;
            result.append(buffer_.data(), remaining);
        }

        return result;
    }
};

} // namespace casket