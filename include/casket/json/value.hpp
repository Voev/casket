#pragma once

#include <vector>
#include <map>
#include <string>
#include <utility>

#include <casket/nonstd/variant.hpp>
#include <casket/nonstd/optional.hpp>
#include <casket/json/types.hpp>

namespace casket::json
{

struct Object;
class Value;

struct Array
{
    std::vector<Value> data_;

    Array() = default;
    explicit Array(size_t n)
        : data_(n)
    {
    }
    Array(const Array&) = default;
    Array(Array&&) noexcept = default;
    Array& operator=(const Array&) = default;
    Array& operator=(Array&&) noexcept = default;
    Array(std::initializer_list<Value> il)
        : data_(il)
    {
    }

    Value& operator[](size_t idx)
    {
        return data_[idx];
    }
    const Value& operator[](size_t idx) const
    {
        return data_[idx];
    }
    size_t size() const noexcept
    {
        return data_.size();
    }
    bool empty() const noexcept
    {
        return data_.empty();
    }

    template <typename T>
    void push_back(T&& value)
    {
        data_.push_back(std::forward<T>(value));
    }

    void reserve(size_t n)
    {
        data_.reserve(n);
    }

    auto begin() noexcept
    {
        return data_.begin();
    }
    auto end() noexcept
    {
        return data_.end();
    }
    auto begin() const noexcept
    {
        return data_.begin();
    }
    auto end() const noexcept
    {
        return data_.end();
    }

    bool operator==(const Array& other) const
    {
        return data_ == other.data_;
    }
    bool operator!=(const Array& other) const
    {
        return !(*this == other);
    }

    const std::vector<Value>& data() const& noexcept
    {
        return data_;
    }
    std::vector<Value>&& data() && noexcept
    {
        return std::move(data_);
    }
};

struct Object
{
    std::map<std::string, Value> fields;

    Object() = default;
    Object(const Object&) = default;
    Object(Object&&) noexcept = default;
    Object& operator=(const Object&) = default;
    Object& operator=(Object&&) noexcept = default;

    template <typename T>
    void insert(std::string key, T&& value)
    {
        fields.emplace(std::move(key), Value{std::forward<T>(value)});
    }

    template <typename T>
    nonstd::optional<T> get(const std::string& key) const noexcept;

    bool isNull(const std::string& key) const noexcept;

    template <typename T>
    T getOr(const std::string& key, const T& defaultValue) const noexcept;

    template <typename T>
    T getOr(const std::string& key, T&& defaultValue) const noexcept;

    bool has(const std::string& key) const noexcept;
    size_t erase(const std::string& key) noexcept;
    size_t size() const noexcept;
    bool empty() const noexcept;

    auto begin() noexcept
    {
        return fields.begin();
    }
    auto end() noexcept
    {
        return fields.end();
    }
    auto begin() const noexcept
    {
        return fields.begin();
    }
    auto end() const noexcept
    {
        return fields.end();
    }

    bool operator==(const Object& other) const;
    bool operator!=(const Object& other) const;
};

class Value
{
public:
    using Variant = nonstd::variant<std::string, Integer, Float, Boolean, Null, Array, Object>;

private:
    Variant data_;

public:
    Value()
        : data_(Null{})
    {
    }
    Value(const Value&) = default;
    Value(Value&&) noexcept = default;
    Value& operator=(const Value&) = default;
    Value& operator=(Value&&) noexcept = default;

    Value(std::string v)
        : data_(std::move(v))
    {
    }
    Value(const char* v)
        : data_(std::string(v))
    {
    }
    Value(Integer v)
        : data_(v)
    {
    }
    Value(Float v)
        : data_(v)
    {
    }
    Value(Boolean v)
        : data_(v)
    {
    }
    Value(Null)
        : data_(Null{})
    {
    }
    Value(Array v)
        : data_(std::move(v))
    {
    }
    Value(Object v)
        : data_(std::move(v))
    {
    }

    template <typename T>
    bool is() const noexcept
    {
        return std::holds_alternative<T>(data_);
    }

    template <typename T>
    const T* get() const noexcept
    {
        return std::get_if<T>(&data_);
    }

    template <typename T>
    T* get() noexcept
    {
        return std::get_if<T>(&data_);
    }

    template <typename T>
    nonstd::optional<T> as() const noexcept;

    template <typename T>
    T&& move() noexcept
    {
        return std::move(std::get<T>(data_));
    }

    template <typename T>
    const T& ref() const& noexcept
    {
        return std::get<T>(data_);
    }

    const Variant& variant() const& noexcept
    {
        return data_;
    }
    Variant&& variant() && noexcept
    {
        return std::move(data_);
    }

    bool operator==(const Value& other) const
    {
        return data_ == other.data_;
    }
    bool operator!=(const Value& other) const
    {
        return !(*this == other);
    }

    std::string toString() const;
    std::string typeName() const;

    bool isNull() const noexcept
    {
        return std::holds_alternative<Null>(data_);
    }
};

template <>
nonstd::optional<std::string> Value::as<std::string>() const noexcept;
template <>
nonstd::optional<Integer> Value::as<Integer>() const noexcept;
template <>
nonstd::optional<int> Value::as<int>() const noexcept;
template <>
nonstd::optional<Float> Value::as<Float>() const noexcept;
template <>
nonstd::optional<Boolean> Value::as<Boolean>() const noexcept;
template <>
nonstd::optional<Null> Value::as<Null>() const noexcept;
template <>
nonstd::optional<Array> Value::as<Array>() const noexcept;
template <>
nonstd::optional<Object> Value::as<Object>() const noexcept;

} // namespace casket::json