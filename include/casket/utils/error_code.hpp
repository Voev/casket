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

inline void SetSystemError(std::error_code& ec, const std::errc e)
{
    ec = std::make_error_code(e);
}

} // namespace casket