#pragma once
#include <casket/nonstd/string_view.hpp>
#include <system_error>
#include <casket/utils/format.hpp>

namespace casket
{

class SystemError final : public std::system_error
{
public:
    SystemError(std::error_code ec)
        : std::system_error(ec)
    {
    }

    SystemError(std::error_code ec, nonstd::string_view what)
        : std::system_error(ec, what.data())
    {
    }

    template <typename... Args>
    SystemError(std::error_code ec, nonstd::string_view fmt, Args&&... args)
        : std::system_error(ec, format(fmt, std::forward<Args>(args)...))
    {
    }
};

class RuntimeError final : public std::runtime_error
{
public:
    RuntimeError(nonstd::string_view what)
        : std::runtime_error(what.data())
    {
    }

    template <typename... Args>
    explicit RuntimeError(nonstd::string_view fmt, Args&&... args)
        : std::runtime_error(format(fmt, std::forward<Args>(args)...))
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
inline void ThrowIfError(std::error_code ec, nonstd::string_view fmt, Args&&... args)
{
    if (ec)
    {
        throw SystemError(ec, fmt, std::forward<Args>(args)...);
    }
}

template <typename... Args>
inline void ThrowIfTrue(bool exprResult, nonstd::string_view fmt, Args&&... args)
{
    if (exprResult)
    {
        throw RuntimeError(fmt, std::forward<Args>(args)...);
    }
}

template <typename... Args>
inline void ThrowIfFalse(bool exprResult, nonstd::string_view fmt, Args&&... args)
{
    ThrowIfTrue(!exprResult, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void ThrowIfTrue(bool exprResult, std::error_code ec, nonstd::string_view fmt, Args&&... args)
{
    if (exprResult)
    {
        throw RuntimeError(ec, fmt, std::forward<Args>(args)...);
    }
}

template <typename... Args>
inline void ThrowIfFalse(bool exprResult, std::error_code ec, nonstd::string_view fmt, Args&&... args)
{
    ThrowIfTrue(!exprResult, ec, fmt, std::forward<Args>(args)...);
}

} // namespace casket