#pragma once
#include <casket/db/i_field_value.hpp>
#include <casket/db/exception.hpp>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <memory>
#include <iomanip>
#include <chrono>
#include <vector>
#include <sstream>

namespace casket::db
{

template <typename T>
class TypedFieldValue : public IFieldValue
{
private:
    T value_;
    bool isNull_;

public:
    TypedFieldValue()
        : value_(T{})
        , isNull_(true)
    {
    }

    TypedFieldValue(const T& val)
        : value_(val)
        , isNull_(false)
    {
    }

    TypedFieldValue(T&& val)
        : value_(std::move(val))
        , isNull_(false)
    {
    }

    std::shared_ptr<IFieldValue> clone() const override
    {
        if (isNull_)
        {
            return std::make_shared<TypedFieldValue<T>>();
        }
        return std::make_shared<TypedFieldValue<T>>(value_);
    }

    bool equals(const IFieldValue& other) const override
    {
        if (getType() != other.getType())
        {
            return false;
        }

        const auto& typedOther = dynamic_cast<const TypedFieldValue<T>&>(other);

        if (isNull_ || typedOther.isNull_)
        {
            return isNull_ == typedOther.isNull_;
        }

        return value_ == typedOther.value_;
    }

    int compare(const IFieldValue& other) const override
    {
        if (getType() != other.getType())
        {
            throw Exception(ErrorType::TYPE_MISMATCH,
                            "Cannot compare different types: " + std::string(getType().name()) + " vs " +
                                std::string(other.getType().name()));
        }

        const auto& typedOther = dynamic_cast<const TypedFieldValue<T>&>(other);

        if (isNull_ && !typedOther.isNull_)
        {
            return -1;
        }
        if (!isNull_ && typedOther.isNull_)
        {
            return 1;
        }
        if (isNull_ && typedOther.isNull_)
        {
            return 0;
        }

        if (value_ < typedOther.value_)
        {
            return -1;
        }
        if (value_ > typedOther.value_)
        {
            return 1;
        }
        return 0;
    }

    std::type_index getType() const override
    {
        return std::type_index(typeid(T));
    }

    size_t hash() const override
    {
        if (isNull_)
        {
            return 0;
        }
        return std::hash<T>()(value_);
    }

    bool isNull() const override
    {
        return isNull_;
    }

    std::string toString() const override
    {
        if (isNull_)
        {
            return "NULL";
        }

        if constexpr (std::is_same_v<T, std::string>)
        {
            return value_;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            return value_ ? "true" : "false";
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                std::string str = std::to_string(value_);
                str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                if (str.back() == '.')
                {
                    str.pop_back();
                }
                return str;
            }
            else
            {
                return std::to_string(value_);
            }
        }
        else
        {
            return "Unsupported type: " + std::string(typeid(T).name());
        }
    }

    const T& getValue() const
    {
        if (isNull_)
        {
            throw Exception(ErrorType::INVALID_CONVERSION, "Cannot get value of NULL field");
        }
        return value_;
    }

    T& getValue()
    {
        if (isNull_)
        {
            throw Exception(ErrorType::INVALID_CONVERSION, "Cannot get value of NULL field");
        }
        return value_;
    }

    operator T() const
    {
        return getValue();
    }

    static std::shared_ptr<TypedFieldValue<T>> createNull()
    {
        return std::make_shared<TypedFieldValue<T>>();
    }

    static std::shared_ptr<TypedFieldValue<T>> create(const T& value)
    {
        return std::make_shared<TypedFieldValue<T>>(value);
    }
};

class TimePointField : public IFieldValue
{
private:
    std::chrono::system_clock::time_point value_;
    bool isNull_;

public:
    TimePointField()
        : isNull_(true)
    {
    }
    TimePointField(const std::chrono::system_clock::time_point& val)
        : value_(val)
        , isNull_(false)
    {
    }
    TimePointField(std::chrono::system_clock::time_point&& val)
        : value_(std::move(val))
        , isNull_(false)
    {
    }

    std::shared_ptr<IFieldValue> clone() const override
    {
        if (isNull_)
            return std::make_shared<TimePointField>();
        return std::make_shared<TimePointField>(value_);
    }

    bool equals(const IFieldValue& other) const override
    {
        if (getType() != other.getType())
            return false;
        const auto& typedOther = dynamic_cast<const TimePointField&>(other);
        if (isNull_ || typedOther.isNull_)
            return isNull_ == typedOther.isNull_;
        return value_ == typedOther.value_;
    }

    int compare(const IFieldValue& other) const override
    {
        if (getType() != other.getType())
            throw Exception(ErrorType::TYPE_MISMATCH, "Cannot compare different types");

        const auto& typedOther = dynamic_cast<const TimePointField&>(other);

        if (isNull_ && !typedOther.isNull_)
            return -1;
        if (!isNull_ && typedOther.isNull_)
            return 1;
        if (isNull_ && typedOther.isNull_)
            return 0;

        if (value_ < typedOther.value_)
            return -1;
        if (value_ > typedOther.value_)
            return 1;
        return 0;
    }

    std::type_index getType() const override
    {
        return std::type_index(typeid(std::chrono::system_clock::time_point));
    }

    size_t hash() const override
    {
        if (isNull_)
            return 0;
        return std::hash<long long>()(value_.time_since_epoch().count());
    }

    bool isNull() const override
    {
        return isNull_;
    }

    std::string toString() const override
    {
        if (isNull_)
            return "NULL";
        auto time = std::chrono::system_clock::to_time_t(value_);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    const std::chrono::system_clock::time_point& getValue() const
    {
        if (isNull_)
            throw Exception(ErrorType::INVALID_CONVERSION, "Value is NULL");
        return value_;
    }

    operator std::chrono::system_clock::time_point() const
    {
        return getValue();
    }
};

class BinaryDataField : public IFieldValue
{
private:
    std::vector<uint8_t> value_;
    bool isNull_;

public:
    BinaryDataField()
        : isNull_(true)
    {
    }
    BinaryDataField(const std::vector<uint8_t>& val)
        : value_(val)
        , isNull_(false)
    {
    }
    BinaryDataField(std::vector<uint8_t>&& val)
        : value_(std::move(val))
        , isNull_(false)
    {
    }
    BinaryDataField(const uint8_t* data, size_t size)
        : value_(data, data + size)
        , isNull_(false)
    {
    }

    std::shared_ptr<IFieldValue> clone() const override
    {
        if (isNull_)
            return std::make_shared<BinaryDataField>();
        return std::make_shared<BinaryDataField>(value_);
    }

    bool equals(const IFieldValue& other) const override
    {
        if (getType() != other.getType())
            return false;
        const auto& typedOther = dynamic_cast<const BinaryDataField&>(other);
        if (isNull_ || typedOther.isNull_)
            return isNull_ == typedOther.isNull_;
        return value_ == typedOther.value_;
    }

    int compare(const IFieldValue& other) const override
    {
        if (getType() != other.getType())
            throw Exception(ErrorType::TYPE_MISMATCH, "Cannot compare different types");

        const auto& typedOther = dynamic_cast<const BinaryDataField&>(other);

        if (isNull_ && !typedOther.isNull_)
            return -1;
        if (!isNull_ && typedOther.isNull_)
            return 1;
        if (isNull_ && typedOther.isNull_)
            return 0;

        if (value_ < typedOther.value_)
            return -1;
        if (value_ > typedOther.value_)
            return 1;
        return 0;
    }

    std::type_index getType() const override
    {
        return std::type_index(typeid(std::vector<uint8_t>));
    }

    size_t hash() const override
    {
        if (isNull_)
            return 0;
        size_t h = 0;
        for (uint8_t b : value_)
        {
            h ^= std::hash<uint8_t>()(b) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }

    bool isNull() const override
    {
        return isNull_;
    }

    std::string toString() const override
    {
        if (isNull_)
            return "NULL";
        std::string hex;
        hex.reserve(value_.size() * 2);
        for (uint8_t byte : value_)
        {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02X", byte);
            hex += buf;
        }
        return hex;
    }

    const std::vector<uint8_t>& getValue() const
    {
        if (isNull_)
            throw Exception(ErrorType::INVALID_CONVERSION, "Value is NULL");
        return value_;
    }

    operator std::vector<uint8_t>() const
    {
        return getValue();
    }

    size_t size() const
    {
        return isNull_ ? 0 : value_.size();
    }
    const uint8_t* data() const
    {
        return isNull_ ? nullptr : value_.data();
    }
};

template <typename T>
std::shared_ptr<IFieldValue> makeFieldValue(const T& value)
{
    return std::make_shared<TypedFieldValue<T>>(value);
}

inline std::shared_ptr<IFieldValue> makeFieldValue(const std::chrono::system_clock::time_point& value)
{
    return std::make_shared<TimePointField>(value);
}

inline std::shared_ptr<IFieldValue> makeFieldValue(const std::vector<uint8_t>& value)
{
    return std::make_shared<BinaryDataField>(value);
}

inline std::shared_ptr<IFieldValue> makeFieldValue(std::vector<uint8_t>&& value)
{
    return std::make_shared<BinaryDataField>(std::move(value));
}

template <typename T>
std::shared_ptr<IFieldValue> makeNullFieldValue()
{
    return std::make_shared<TypedFieldValue<T>>();
}

template <>
inline std::shared_ptr<IFieldValue> makeFieldValue<std::nullptr_t>(const std::nullptr_t&)
{
    return std::make_shared<TypedFieldValue<int>>();
}

} // namespace casket::db