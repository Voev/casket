#pragma once
#include <algorithm>
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

inline std::string replace(const std::string& s, const std::string& what, const std::string& with) {
    std::string data = s;

    size_t pos = data.find(what);
    while (pos != std::string::npos) {
        data.replace(pos, what.size(), with);
        pos = data.find(what, pos + with.size());
    }

    return data;
}

inline std::vector<std::string> split(const std::string& str,
                                      const std::string& delim)
{
    size_t start = 0;
    size_t end = std::string::npos;
    size_t delimLen = delim.length();

    std::string token;
    std::vector<std::string> res;

    while ((end = str.find(delim, start)) != std::string::npos)
    {
        token = str.substr(start, end - start);
        start = end + delimLen;
        res.push_back(token);
    }

    res.push_back(str.substr(start));
    return res;
}

inline std::string join(const std::vector<std::string>& strings,
                        const std::string& delim)
{
    std::string res;
    for (auto it = strings.cbegin(); it != strings.cend(); ++it)
    {
        if (it != strings.begin())
        {
            res.append(delim);
        }
        res.append(*it);
    }
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
