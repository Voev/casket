// value_parser.hpp
#pragma once

#include <sstream>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <cstdint>
#include <type_traits>

#include <casket/nonstd/string_view.hpp>
#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

// ============================================================================
// Базовый шаблон (fallback)
// ============================================================================
template <typename T, typename Enable = void>
struct ValueParserImpl
{
    static T parse(nonstd::string_view str)
    {
        std::istringstream iss{std::string(str)};
        T value;
        if (!(iss >> value))
        {
            throw RuntimeError("could not parse value '{}'", str);
        }
        return value;
    }
};

// ============================================================================
// Вспомогательные трейты
// ============================================================================
template <typename T>
struct is_integer : std::false_type
{
};

template <>
struct is_integer<short> : std::true_type
{
};
template <>
struct is_integer<int> : std::true_type
{
};
template <>
struct is_integer<long> : std::true_type
{
};
template <>
struct is_integer<long long> : std::true_type
{
};
template <>
struct is_integer<unsigned short> : std::true_type
{
};
template <>
struct is_integer<unsigned int> : std::true_type
{
};
template <>
struct is_integer<unsigned long> : std::true_type
{
};
template <>
struct is_integer<unsigned long long> : std::true_type
{
};
template <>
struct is_integer<signed char> : std::true_type
{
};
template <>
struct is_integer<unsigned char> : std::true_type
{
};

template <typename T>
struct is_floating_point : std::false_type
{
};

template <>
struct is_floating_point<float> : std::true_type
{
};
template <>
struct is_floating_point<double> : std::true_type
{
};
template <>
struct is_floating_point<long double> : std::true_type
{
};

template <typename T>
class IntegerParser
{
public:
    static T parse(nonstd::string_view str)
    {
        if (str.empty())
        {
            throw RuntimeError("empty integer value");
        }

        // Проверка на отрицательное для беззнаковых
        if (std::is_unsigned<T>::value && !str.empty() && str[0] == '-')
        {
            throw RuntimeError("unsigned type cannot be negative: '{}'", str);
        }

        // Пропускаем пробелы в начале
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
        {
            str.remove_prefix(1);
        }
        
        // Пропускаем пробелы в конце
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
        {
            str.remove_suffix(1);
        }

        if (str.empty())
        {
            throw RuntimeError("empty integer value after trim");
        }

        std::string s(str);
        char* endptr = nullptr;
        
        if (std::is_signed<T>::value)
        {
            long long value = std::strtoll(s.c_str(), &endptr, 10);
            
            if (endptr == s.c_str())
            {
                throw RuntimeError("no digits parsed in '{}'", str);
            }
            if (*endptr != '\0')
            {
                throw RuntimeError("invalid characters in '{}'", str);
            }
            
            long long minVal = static_cast<long long>(std::numeric_limits<T>::min());
            long long maxVal = static_cast<long long>(std::numeric_limits<T>::max());
            
            if (value < minVal || value > maxVal)
            {
                throw RuntimeError("value '{}' out of range", str);
            }
            
            return static_cast<T>(value);
        }
        else
        {
            unsigned long long value = std::strtoull(s.c_str(), &endptr, 10);
            
            if (endptr == s.c_str())
            {
                throw RuntimeError("no digits parsed in '{}'", str);
            }
            if (*endptr != '\0')
            {
                throw RuntimeError("invalid characters in '{}'", str);
            }
            
            unsigned long long maxVal = static_cast<unsigned long long>(std::numeric_limits<T>::max());
            
            if (value > maxVal)
            {
                throw RuntimeError("value '{}' out of range", str);
            }
            
            return static_cast<T>(value);
        }
    }
};

template <typename T>
class FloatParser
{
public:
    static T parse(nonstd::string_view str)
    {
        if (str.empty())
        {
            throw RuntimeError("empty float value");
        }

        // Пропускаем пробелы
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
        {
            str.remove_prefix(1);
        }

        if (str.empty())
        {
            throw RuntimeError("empty float value after trim");
        }

        std::string s(str);
        char* endptr = nullptr;

        long double value = std::strtold(s.c_str(), &endptr);

        if (endptr == s.c_str())
        {
            throw RuntimeError("no digits parsed in '{}'", str);
        }
        if (*endptr != '\0')
        {
            throw RuntimeError("invalid characters in '{}'", str);
        }

        if (value < std::numeric_limits<T>::lowest() || value > std::numeric_limits<T>::max())
        {
            throw RuntimeError("value '{}' out of range", str);
        }

        return static_cast<T>(value);
    }
};

// ============================================================================
// Специализация для целых чисел
// ============================================================================
template <typename T>
struct ValueParserImpl<T, typename std::enable_if<is_integer<T>::value>::type>
{
    static T parse(nonstd::string_view str)
    {
        return IntegerParser<T>::parse(str);
    }
};

// ============================================================================
// Специализация для чисел с плавающей точкой
// ============================================================================
template <typename T>
struct ValueParserImpl<T, typename std::enable_if<is_floating_point<T>::value>::type>
{
    static T parse(nonstd::string_view str)
    {
        return FloatParser<T>::parse(str);
    }
};

// ============================================================================
// Специализация для bool
// ============================================================================
template <>
struct ValueParserImpl<bool, void>
{
    static bool parse(nonstd::string_view str)
    {
        if (str.empty())
        {
            throw RuntimeError("empty bool value");
        }

        // Пропускаем пробелы
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
        {
            str.remove_prefix(1);
        }
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
        {
            str.remove_suffix(1);
        }

        if (str.empty())
        {
            throw RuntimeError("empty bool value after trim");
        }

        char c = std::tolower(static_cast<unsigned char>(str[0]));

        if (c == 't' || c == 'y' || c == '1')
        {
            if ((str.size() == 4 && iequals(str, "true")) || (str.size() == 3 && iequals(str, "yes")) ||
                (str.size() == 1 && str[0] == '1'))
            {
                return true;
            }
        }
        else if (c == 'f' || c == 'n' || c == '0')
        {
            if ((str.size() == 5 && iequals(str, "false")) || (str.size() == 2 && iequals(str, "no")) ||
                (str.size() == 1 && str[0] == '0'))
            {
                return false;
            }
        }
        else if (c == 'o')
        {
            if (str.size() == 2 && iequals(str, "on"))
                return true;
            if (str.size() == 3 && iequals(str, "off"))
                return false;
        }

        throw RuntimeError("could not parse bool value '{}'", str);
    }
};

// ============================================================================
// Специализация для std::string
// ============================================================================
template <>
struct ValueParserImpl<std::string, void>
{
    static std::string parse(nonstd::string_view str)
    {
        // Пропускаем пробелы
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
        {
            str.remove_prefix(1);
        }
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
        {
            str.remove_suffix(1);
        }

        // Удаляем кавычки
        if (str.size() >= 2 &&
            ((str.front() == '"' && str.back() == '"') || (str.front() == '\'' && str.back() == '\'')))
        {
            str = str.substr(1, str.size() - 2);

            // Обрабатываем escape последовательности
            std::string result;
            result.reserve(str.size());

            for (size_t i = 0; i < str.size(); ++i)
            {
                if (str[i] == '\\' && i + 1 < str.size())
                {
                    char next = str[i + 1];
                    switch (next)
                    {
                    case 'n':
                        result += '\n';
                        break;
                    case 't':
                        result += '\t';
                        break;
                    case 'r':
                        result += '\r';
                        break;
                    case '\\':
                        result += '\\';
                        break;
                    case '"':
                        result += '"';
                        break;
                    case '\'':
                        result += '\'';
                        break;
                    default:
                        result += next;
                        break;
                    }
                    ++i;
                }
                else
                {
                    result += str[i];
                }
            }

            return result;
        }

        return std::string(str);
    }
};

// ============================================================================
// Специализация для std::chrono::milliseconds
// ============================================================================
template <>
struct ValueParserImpl<std::chrono::milliseconds, void>
{
    static std::chrono::milliseconds parse(nonstd::string_view str)
    {
        if (str.empty())
        {
            throw RuntimeError("empty duration string");
        }

        // Пропускаем пробелы
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
        {
            str.remove_prefix(1);
        }
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())))
        {
            str.remove_suffix(1);
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
            suffix = std::string(str.substr(i));
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

// ============================================================================
// Основной интерфейс ValueParser
// ============================================================================
template <typename T>
struct ValueParser
{
    static T parse(nonstd::string_view str)
    {
        return ValueParserImpl<T>::parse(str);
    }
};

} // namespace casket::opt