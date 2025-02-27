#pragma once
#include <string_view>
#include <system_error>
#include <casket/utils/format.hpp>

namespace casket::utils
{

class SystemError final : public std::system_error
{
public:
    SystemError(std::error_code ec)
        : std::system_error(ec)
    {
    }

    SystemError(std::error_code ec, std::string_view what)
        : std::system_error(ec, what.data())
    {
    }

    template <typename... Args>
    SystemError(std::error_code ec, std::string_view format, Args&&... args)
        : std::system_error(ec, utils::format(format, std::forward<Args>(args)...))
    {
    }
};

class RuntimeError final : public std::runtime_error
{
public:
    RuntimeError(std::string_view what)
        : std::runtime_error(what.data())
    {
    }

    template <typename... Args>
    explicit RuntimeError(std::string_view format, Args&&... args)
        : std::runtime_error(utils::format(format, std::forward<Args>(args)...))
    {
    }
};

inline void ThrowIfError(std::error_code ec)
{
    if (ec)
    {
        throw SystemError(ec);
    }
}

template <typename... Args>
inline void ThrowIfError(std::error_code ec, std::string_view format, Args&&... args)
{
    if (ec)
    {
        throw SystemError(ec, format, std::forward<Args>(args)...);
    }
}

template <typename... Args>
inline void ThrowIfTrue(bool exprResult, std::string_view format, Args&&... args)
{
    if (exprResult)
    {
        throw RuntimeError(format, std::forward<Args>(args)...);
    }
}

template <typename... Args>
inline void ThrowIfFalse(bool exprResult, std::string_view format, Args&&... args)
{
    ThrowIfTrue(!exprResult, format, std::forward<Args>(args)...);
}

template <typename... Args>
inline void ThrowIfTrue(bool exprResult, std::error_code ec, std::string_view format, Args&&... args)
{
    if (exprResult)
    {
        throw RuntimeError(ec, format, std::forward<Args>(args)...);
    }
}

template <typename... Args>
inline void ThrowIfFalse(bool exprResult, std::error_code ec, std::string_view format, Args&&... args)
{
    ThrowIfTrue(!exprResult, ec, format, std::forward<Args>(args)...);
}

} // namespace casket::utils