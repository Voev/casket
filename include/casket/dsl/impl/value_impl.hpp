#pragma once

#include <sstream>
#include <casket/dsl/value.hpp>

namespace casket::dsl
{

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