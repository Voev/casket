#pragma once
#include <sstream>
#include <string>

namespace casket
{

namespace detail
{

#if (__cplusplus >= 201703L)

template <typename T>
void formatHelper(std::ostringstream& oss, nonstd::string_view& str, const T& value)
{
    std::size_t openBracket = str.find('{');
    if (openBracket == std::string::npos)
    {
        return;
    }

    std::size_t closeBracket = str.find('}', openBracket + 1);
    if (closeBracket == std::string::npos)
    {
        return;
    }

    oss << str.substr(0, openBracket) << value;
    str = str.substr(closeBracket + 1);
}

#else // (__cplusplus >= 201703L)

template <typename Arg>
void formatHelper(std::ostringstream& oss, nonstd::string_view& str, const Arg& arg)
{
    auto pos = str.find("{}");
    if (pos != nonstd::string_view::npos)
    {
        oss << str.substr(0, pos) << arg;
        str = str.substr(pos + 2);
    }
}

void formatHelper(std::ostringstream&, nonstd::string_view&)
{
}

template <typename Arg, typename... Args>
void formatHelper(std::ostringstream& oss, nonstd::string_view& str, const Arg& arg, const Args&... args)
{
    formatHelper(oss, str, arg);
    formatHelper(oss, str, args...);
}

#endif // (__cplusplus < 201703L)

} // namespace detail

#if (__cplusplus >= 201703L)

template <typename... Args>
std::string format(nonstd::string_view str, Args... args)
{
    std::ostringstream oss;
    (detail::formatHelper(oss, str, args), ...);
    oss << str;
    return oss.str();
}

#else // (__cplusplus >= 201703L)

template <typename... Args>
std::string format(nonstd::string_view str, Args... args)
{
    std::ostringstream oss;
    detail::formatHelper(oss, str, args...);
    oss << str;
    return oss.str();
}

#endif // (__cplusplus < 201703L)

} // namespace casket