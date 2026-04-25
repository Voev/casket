#pragma once

#include <sstream>
#include <chrono>
#include <cctype>
#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

template <typename T>
struct ValueParser
{
    static T parse(const std::string& str)
    {
        std::istringstream iss(str);
        T value;
        if (!(iss >> value))
        {
            throw RuntimeError("could not parse value '{}'", str);
        }
        return value;
    }
};

template <>
struct ValueParser<bool>
{
    static bool parse(const std::string& str)
    {
        if (iequals(str, "true") || iequals(str, "yes") || iequals(str, "1"))
        {
            return true;
        }
        if (iequals(str, "false") || iequals(str, "no") || iequals(str, "0"))
        {
            return false;
        }
        throw RuntimeError("could not parse bool value '{}'", str);
    }
};

template <>
struct ValueParser<std::chrono::milliseconds>
{
    static std::chrono::milliseconds parse(const std::string& str)
    {
        if (str.empty())
        {
            throw RuntimeError("empty duration string");
        }

        long long num = 0;
        std::string numberPart;
        std::string suffix;

        size_t i = 0;
        while (i < str.size() && (std::isdigit(str[i]) || str[i] == '-'))
        {
            numberPart += str[i++];
        }

        if (i < str.size())
        {
            suffix = str.substr(i);
            for (auto& c : suffix)
                c = std::tolower(c);
        }

        if (numberPart.empty())
        {
            throw RuntimeError("could not parse duration value '{}'", str);
        }

        num = std::stoll(numberPart);

        if (suffix == "ms" || suffix.empty())
        {
            return std::chrono::milliseconds(num);
        }
        if (suffix == "s")
        {
            return std::chrono::milliseconds(num * 1000);
        }
        if (suffix == "m")
        {
            return std::chrono::milliseconds(num * 60 * 1000);
        }
        if (suffix == "h")
        {
            return std::chrono::milliseconds(num * 60 * 60 * 1000);
        }

        throw RuntimeError("unknown duration suffix '{}'", suffix);
    }
};

} // namespace casket::opt