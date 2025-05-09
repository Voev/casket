#pragma once
#include <algorithm>
#include <cctype>
#include <vector>
#include <string>
#include <string_view>

namespace casket::utils
{

inline bool equals(std::string_view a, std::string_view b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

inline bool iequals(std::string_view a, std::string_view b)
{
    auto compare = [](char x, char y) -> bool {
        return std::tolower(static_cast<unsigned char>(x)) ==
               std::tolower(static_cast<unsigned char>(y));
    };
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), compare);
}

template <typename String>
inline std::vector<String> split(const String& str, const std::string& delim)
{
    size_t start = 0;
    size_t end = String::npos;
    size_t delimLen = delim.length();

    std::string token;
    std::vector<String> res;

    while ((end = str.find(delim, start)) != String::npos)
    {
        token = str.substr(start, end - start);
        start = end + delimLen;
        res.push_back(token);
    }

    res.push_back(str.substr(start));
    return res;
}

template <typename String>
inline void ltrim(String& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
}

template <typename String>
inline void rtrim(String& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

template <typename String>
inline void trim(String& s)
{
    rtrim<String>(s);
    ltrim<String>(s);
}

} // namespace casket::utils
