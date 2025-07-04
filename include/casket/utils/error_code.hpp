#pragma once
#include <system_error>

namespace casket
{

inline std::error_code GetLastSystemError()
{
    return std::make_error_code(static_cast<std::errc>(errno));
}

inline void ClearError(std::error_code& ec)
{
    ec.assign(0, ec.category());
}

} // namespace casket