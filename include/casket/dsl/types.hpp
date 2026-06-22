#pragma once

#include <cstdint>
#include <string>

namespace casket::dsl
{

using Integer = int64_t;
using Float = double;
using Boolean = bool;

struct Null
{
    constexpr bool operator==(const Null&) const noexcept
    {
        return true;
    }
    constexpr bool operator!=(const Null&) const noexcept
    {
        return false;
    }
};

} // namespace casket::dsl