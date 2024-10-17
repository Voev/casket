#pragma once
#include <charconv>
#include <system_error>
#include <string_view>
#include <type_traits>

#include <casket/utils/exception.hpp>

namespace casket::utils
{

template <typename T>
inline T toNumber(std::string_view value, std::error_code& ec)
{
    static_assert(std::is_integral_v<T> == true);
    T result{};
    auto r = std::from_chars(value.data(), value.data() + value.size(), result);
    if (r.ec != std::errc())
    {
        ec = std::make_error_code(r.ec);
    }
    return result;
}

template <typename T>
inline T toNumber(std::string_view value)
{
    std::error_code ec;
    auto result = toNumber<T>(value, ec);
    utils::ThrowIfError(ec);
    return result;
}

} // namespace casket::utils