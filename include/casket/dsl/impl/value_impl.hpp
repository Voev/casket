#pragma once

#include <casket/dsl/impl/value_impl.hpp>
#include <sstream>

namespace casket::dsl
{

// ============================================================================
// Object методы (реализация)
// ============================================================================
template <typename T>
inline std::optional<T> Object::get(const std::string& key) const noexcept
{
    auto it = fields.find(key);
    if (it == fields.end())
        return std::nullopt;
    return it->second.as<T>();
}

inline bool Object::isNull(const std::string& key) const noexcept
{
    auto it = fields.find(key);
    if (it == fields.end())
        return false;
    return it->second.isNull();
}

template <typename T>
inline T Object::getOr(const std::string& key, const T& defaultValue) const noexcept
{
    auto val = get<T>(key);
    return val.has_value() ? *val : defaultValue;
}

template <typename T>
inline T Object::getOr(const std::string& key, T&& defaultValue) const noexcept
{
    auto val = get<T>(key);
    return val.has_value() ? std::move(*val) : std::forward<T>(defaultValue);
}

inline bool Object::has(const std::string& key) const noexcept
{
    return fields.find(key) != fields.end();
}

inline size_t Object::erase(const std::string& key) noexcept
{
    return fields.erase(key);
}

inline size_t Object::size() const noexcept
{
    return fields.size();
}

inline bool Object::empty() const noexcept
{
    return fields.empty();
}

inline bool Object::operator==(const Object& other) const
{
    return fields == other.fields;
}

inline bool Object::operator!=(const Object& other) const
{
    return !(*this == other);
}

template <>
inline std::optional<std::string> Value::as<std::string>() const noexcept
{
    if (const auto* v = std::get_if<std::string>(&data_))
        return *v;
    return std::nullopt;
}

template <>
inline std::optional<Integer> Value::as<Integer>() const noexcept
{
    if (const auto* v = std::get_if<Integer>(&data_))
        return *v;
    return std::nullopt;
}

template <>
inline std::optional<int> Value::as<int>() const noexcept
{
    if (const auto* v = std::get_if<Integer>(&data_))
        return static_cast<int>(*v);
    return std::nullopt;
}

template <>
inline std::optional<Float> Value::as<Float>() const noexcept
{
    if (const auto* v = std::get_if<Float>(&data_))
        return *v;
    if (const auto* v = std::get_if<Integer>(&data_))
        return static_cast<Float>(*v);
    return std::nullopt;
}

template <>
inline std::optional<Boolean> Value::as<Boolean>() const noexcept
{
    if (const auto* v = std::get_if<Boolean>(&data_))
        return *v;
    return std::nullopt;
}

template <>
inline std::optional<Null> Value::as<Null>() const noexcept
{
    if (std::holds_alternative<Null>(data_))
        return Null{};
    return std::nullopt;
}

template <>
inline std::optional<Array> Value::as<Array>() const noexcept
{
    if (const auto* v = std::get_if<Array>(&data_))
        return *v;
    return std::nullopt;
}

template <>
inline std::optional<Object> Value::as<Object>() const noexcept
{
    if (const auto* v = std::get_if<Object>(&data_))
        return *v;
    return std::nullopt;
}

// ============================================================================
// Value::toString
// ============================================================================
inline std::string Value::toString() const
{
    return std::visit(
        [](auto&& arg) -> std::string
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>)
                return "\"" + arg + "\"";
            else if constexpr (std::is_same_v<T, Integer>)
                return std::to_string(arg);
            else if constexpr (std::is_same_v<T, Float>)
            {
                std::string s = std::to_string(arg);
                while (s.find('.') != std::string::npos && (s.back() == '0' || s.back() == '.'))
                    s.pop_back();
                return s;
            }
            else if constexpr (std::is_same_v<T, Boolean>)
                return arg ? "true" : "false";
            else if constexpr (std::is_same_v<T, Null>)
                return "null";
            else if constexpr (std::is_same_v<T, Array>)
                return "[array]";
            else if constexpr (std::is_same_v<T, Object>)
                return "{object}";
            else
                return "unknown";
        },
        data_);
}

// ============================================================================
// Value::typeName
// ============================================================================
inline std::string Value::typeName() const
{
    return std::visit(
        [](auto&& arg) -> std::string
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>)
                return "string";
            else if constexpr (std::is_same_v<T, Integer>)
                return "integer";
            else if constexpr (std::is_same_v<T, Float>)
                return "float";
            else if constexpr (std::is_same_v<T, Boolean>)
                return "boolean";
            else if constexpr (std::is_same_v<T, Null>)
                return "null";
            else if constexpr (std::is_same_v<T, Array>)
                return "array";
            else if constexpr (std::is_same_v<T, Object>)
                return "object";
            else
                return "unknown";
        },
        data_);
}

} // namespace casket::dsl