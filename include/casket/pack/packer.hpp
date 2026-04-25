#pragma once
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#include <casket/pack/types.hpp>
#include <casket/utils/result.hpp>
#include <casket/nonstd/string_view.hpp>

namespace casket
{

enum class PackerError
{
    BufferOverflow,
    InvalidType,
    InvalidState
};

template <typename T>
using PackResult = Result<T, PackerError>;

class Packer
{
private:
    uint8_t* buffer_;
    size_t capacity_;
    size_t pos_ = 0;

    bool ensure(size_t need)
    {
        if (pos_ + need > capacity_)
            return false;
        return true;
    }

    void writeRaw(const void* data, size_t size)
    {
        memcpy(buffer_ + pos_, data, size);
        pos_ += size;
    }

    void writeTag(TypeTag tag)
    {
        writeRaw(&tag, 1);
    }

    template <typename T>
    void writeInteger(T value)
    {
        writeRaw(&value, sizeof(T));
    }

public:
    Packer(uint8_t* buffer, size_t capacity)
        : buffer_(buffer)
        , capacity_(capacity)
    {
    }

    size_t position() const
    {
        return pos_;
    }
    const uint8_t* data() const
    {
        return buffer_;
    }
    void reset()
    {
        pos_ = 0;
    }
    size_t capacity() const
    {
        return capacity_;
    }
    size_t remaining() const
    {
        return capacity_ - pos_;
    }

    PackResult<Packer*> packNil()
    {
        if (!ensure(1))
        {
            return PackResult<Packer*>(PackerError::BufferOverflow);
        }
        writeTag(TypeTag::Nil);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(bool value)
    {
        if (!ensure(1))
        {
            return PackResult<Packer*>(PackerError::BufferOverflow);
        }
        writeTag(value ? TypeTag::True : TypeTag::False);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(int8_t value)
    {
        if (!ensure(2))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::Int8);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(int16_t value)
    {
        if (!ensure(3))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::Int16);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(int32_t value)
    {
        if (!ensure(5))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::Int32);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(int64_t value)
    {
        if (!ensure(9))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::Int64);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(uint8_t value)
    {
        if (!ensure(2))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::UInt8);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(uint16_t value)
    {
        if (!ensure(3))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::UInt16);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(uint32_t value)
    {
        if (!ensure(5))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::UInt32);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(uint64_t value)
    {
        if (!ensure(9))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::UInt64);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(float value)
    {
        if (!ensure(5))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::Float32);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(double value)
    {
        if (!ensure(9))
            return PackResult<Packer*>(PackerError::BufferOverflow);
        writeTag(TypeTag::Float64);
        writeInteger(value);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> pack(const char* str)
    {
        return pack(nonstd::string_view(str));
    }

    PackResult<Packer*> pack(const std::string& str)
    {
        return pack(nonstd::string_view(str));
    }

    PackResult<Packer*> pack(nonstd::string_view str)
    {
        size_t len = str.size();

        if (len <= 0xFF)
        {
            if (!ensure(2 + len))
                return PackResult<Packer*>(PackerError::BufferOverflow);
            writeTag(TypeTag::Str8);
            writeInteger(static_cast<uint8_t>(len));
        }
        else if (len <= 0xFFFF)
        {
            if (!ensure(3 + len))
                return PackResult<Packer*>(PackerError::BufferOverflow);
            writeTag(TypeTag::Str16);
            writeInteger(static_cast<uint16_t>(len));
        }
        else
        {
            if (!ensure(5 + len))
                return PackResult<Packer*>(PackerError::BufferOverflow);
            writeTag(TypeTag::Str32);
            writeInteger(static_cast<uint32_t>(len));
        }

        writeRaw(str.data(), len);
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> packArrayStart(size_t size)
    {
        if (size <= 0xFFFF)
        {
            if (!ensure(3))
                return PackResult<Packer*>(PackerError::BufferOverflow);
            writeTag(TypeTag::Array16);
            writeInteger(static_cast<uint16_t>(size));
        }
        else
        {
            if (!ensure(5))
                return PackResult<Packer*>(PackerError::BufferOverflow);
            writeTag(TypeTag::Array32);
            writeInteger(static_cast<uint32_t>(size));
        }
        return PackResult<Packer*>(this);
    }

    PackResult<Packer*> packMapStart(size_t size)
    {
        if (size <= 0xFFFF)
        {
            if (!ensure(3))
                return PackResult<Packer*>(PackerError::BufferOverflow);
            writeTag(TypeTag::Map16);
            writeInteger(static_cast<uint16_t>(size));
        }
        else
        {
            if (!ensure(5))
                return PackResult<Packer*>(PackerError::BufferOverflow);
            writeTag(TypeTag::Map32);
            writeInteger(static_cast<uint32_t>(size));
        }
        return PackResult<Packer*>(this);
    }
};

} // namespace casket