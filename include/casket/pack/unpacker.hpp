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

enum class UnpackerError
{
    BufferOverflow,
    UnexpectedTag,
    PrematureEnd,
    InvalidArraySize,
    TypeMismatch,
    ChecksumMismatch,
    InvalidMapSize
};

template <typename T>
using UnpackResult = Result<T, UnpackerError>;

class Unpacker
{
private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;

    bool ensure(size_t need)
    {
        return pos_ + need <= size_;
    }

    uint8_t readTag()
    {
        return data_[pos_++];
    }

    template <typename T>
    T readInteger()
    {
        T value;
        memcpy(&value, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return value;
    }

    template <typename T>
    UnpackResult<T> readTypedInteger(TypeTag expected)
    {
        if (!ensure(1))
            return UnpackResult<T>(UnpackerError::PrematureEnd);
        uint8_t tag = readTag();
        if (tag != static_cast<uint8_t>(expected))
            return UnpackResult<T>(UnpackerError::TypeMismatch);
        if (!ensure(sizeof(T)))
            return UnpackResult<T>(UnpackerError::PrematureEnd);
        return UnpackResult<T>(readInteger<T>());
    }

public:
    Unpacker(const uint8_t* data, size_t size)
        : data_(data)
        , size_(size)
    {
    }

    bool hasMore() const
    {
        return pos_ < size_;
    }
    size_t position() const
    {
        return pos_;
    }
    size_t remaining() const
    {
        return size_ - pos_;
    }

    UnpackResult<bool> unpackBool()
    {
        if (!ensure(1))
            return UnpackResult<bool>(UnpackerError::PrematureEnd);
        uint8_t tag = readTag();
        if (tag == static_cast<uint8_t>(TypeTag::True))
            return UnpackResult<bool>(true);
        if (tag == static_cast<uint8_t>(TypeTag::False))
            return UnpackResult<bool>(false);
        return UnpackResult<bool>(UnpackerError::TypeMismatch);
    }

    UnpackResult<int8_t> unpackInt8()
    {
        return readTypedInteger<int8_t>(TypeTag::Int8);
    }

    UnpackResult<int16_t> unpackInt16()
    {
        return readTypedInteger<int16_t>(TypeTag::Int16);
    }

    UnpackResult<int32_t> unpackInt32()
    {
        return readTypedInteger<int32_t>(TypeTag::Int32);
    }

    UnpackResult<int64_t> unpackInt64()
    {
        return readTypedInteger<int64_t>(TypeTag::Int64);
    }

    UnpackResult<uint8_t> unpackUInt8()
    {
        return readTypedInteger<uint8_t>(TypeTag::UInt8);
    }

    UnpackResult<uint16_t> unpackUInt16()
    {
        return readTypedInteger<uint16_t>(TypeTag::UInt16);
    }

    UnpackResult<uint32_t> unpackUInt32()
    {
        return readTypedInteger<uint32_t>(TypeTag::UInt32);
    }

    UnpackResult<uint64_t> unpackUInt64()
    {
        return readTypedInteger<uint64_t>(TypeTag::UInt64);
    }

    UnpackResult<float> unpackFloat()
    {
        return readTypedInteger<float>(TypeTag::Float32);
    }

    UnpackResult<double> unpackDouble()
    {
        return readTypedInteger<double>(TypeTag::Float64);
    }

    UnpackResult<nonstd::string_view> unpackString()
    {
        if (!ensure(1))
            return UnpackResult<nonstd::string_view>(UnpackerError::PrematureEnd);
        uint8_t tag = readTag();

        size_t len = 0;
        if (tag == static_cast<uint8_t>(TypeTag::Str8))
        {
            if (!ensure(1))
                return UnpackResult<nonstd::string_view>(UnpackerError::PrematureEnd);
            len = readInteger<uint8_t>();
        }
        else if (tag == static_cast<uint8_t>(TypeTag::Str16))
        {
            if (!ensure(2))
                return UnpackResult<nonstd::string_view>(UnpackerError::PrematureEnd);
            len = readInteger<uint16_t>();
        }
        else if (tag == static_cast<uint8_t>(TypeTag::Str32))
        {
            if (!ensure(4))
                return UnpackResult<nonstd::string_view>(UnpackerError::PrematureEnd);
            len = readInteger<uint32_t>();
        }
        else
        {
            return UnpackResult<nonstd::string_view>(UnpackerError::TypeMismatch);
        }

        if (!ensure(len))
            return UnpackResult<nonstd::string_view>(UnpackerError::PrematureEnd);
        std::string_view str(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return UnpackResult<nonstd::string_view>(str);
    }

    UnpackResult<size_t> unpackArraySize()
    {
        if (!ensure(1))
            return UnpackResult<size_t>(UnpackerError::PrematureEnd);
        uint8_t tag = readTag();

        if (tag == static_cast<uint8_t>(TypeTag::Array16))
        {
            if (!ensure(2))
                return UnpackResult<size_t>(UnpackerError::PrematureEnd);
            return UnpackResult<size_t>(readInteger<uint16_t>());
        }
        else if (tag == static_cast<uint8_t>(TypeTag::Array32))
        {
            if (!ensure(4))
                return UnpackResult<size_t>(UnpackerError::PrematureEnd);
            return UnpackResult<size_t>(readInteger<uint32_t>());
        }

        return UnpackResult<size_t>(UnpackerError::TypeMismatch);
    }

    UnpackResult<size_t> unpackMapSize()
    {
        if (!ensure(1))
            return UnpackResult<size_t>(UnpackerError::PrematureEnd);
        uint8_t tag = readTag();

        if (tag == static_cast<uint8_t>(TypeTag::Map16))
        {
            if (!ensure(2))
                return UnpackResult<size_t>(UnpackerError::PrematureEnd);
            return UnpackResult<size_t>(readInteger<uint16_t>());
        }
        else if (tag == static_cast<uint8_t>(TypeTag::Map32))
        {
            if (!ensure(4))
                return UnpackResult<size_t>(UnpackerError::PrematureEnd);
            return UnpackResult<size_t>(readInteger<uint32_t>());
        }

        return UnpackResult<size_t>(UnpackerError::TypeMismatch);
    }

    template <typename T>
    UnpackResult<T> unpack()
    {
        return T::unpack(*this);
    }
};

} // namespace snet