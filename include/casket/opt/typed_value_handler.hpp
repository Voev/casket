/// @file
/// @brief Typed option value handler class definition.

#pragma once
#include <memory>
#include <sstream>
#include <type_traits>

#include <casket/opt/option_value_handler.hpp>
#include <casket/opt/value_parser.hpp>
#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

/// @brief Typed option value handler.
template <typename T>
class TypedValueHandler : public OptionValueHandler
{
public:
    /// @brief Constructor that binds the typed value to a pointer.
    ///
    /// @param typedPtr A pointer to the storage where the parsed value will
    ///                 be stored.
    TypedValueHandler(T* typedPtr = nullptr)
        : typedPtr_(typedPtr)
    {
    }

    /// @brief Parses arguments into a value of type `T`.
    ///
    /// This method attempts to convert the arguments into a value
    /// of type `T`. If successful, it stores the result in an `std::any`
    /// object. Throws an exception if parsing fails.
    ///
    /// For string types, multiple arguments are joined with spaces.
    /// For other types, only the first argument is used.
    ///
    /// @param value The `std::any` object where the parsed value is stored.
    /// @param args Span of string_view arguments to parse.
    ///
    /// @throws std::runtime_error If the arguments cannot be parsed into type `T`.
    void parse(nonstd::any& value, nonstd::span<const nonstd::string_view> args) override
    {
        if (args.size() < minTokens())
        {
            ThrowIfTrue(true, "Expected at least {} value(s), got {}", minTokens(), args.size());
        }

        if (args.size() > maxTokens())
        {
            ThrowIfTrue(true, "Expected at most {} value(s), got {}", maxTokens(), args.size());
        }

        if (args.empty())
        {
            if constexpr (std::is_default_constructible_v<T>)
            {
                value = T();
            }
            else
            {
                ThrowIfTrue(true, "Empty value not allowed for this type");
            }
            return;
        }

        T parsedValue = ValueParser<T>::parse(args[0]);
        value = std::move(parsedValue);
    }

    /// @brief Notification callback for when the value is set.
    ///
    /// This method ensures that if a value is successfully parsed, it is
    /// stored in the provided pointer to hold the typed value. If the pointer
    /// is null or the value is of a wrong type, no action is taken.
    ///
    /// @param valueStore The `std::any` containing the parsed value of type `T`.
    void notify(const nonstd::any& valueStore) const override
    {
        const T* value = nonstd::any_cast<T>(&valueStore);
        if (typedPtr_ && value)
        {
            *typedPtr_ = *value;
        }
    }

    /// @brief Returns the minimum number of tokens required for parsing.
    ///
    /// For `TypedValue`, one token is required for non-string types.
    /// For string types, zero tokens are allowed (empty string).
    ///
    /// @return Returns 0 for string types, 1 for others.
    std::size_t minTokens() const override
    {
        return 1;
    }

    /// @brief Returns the maximum number of tokens allowed for parsing.
    ///
    /// For string types, accepts unlimited tokens (joins them).
    /// For other types, only accepts 1 token.
    ///
    /// @return Returns 1 for non-string types, unlimited for strings.
    std::size_t maxTokens() const override
    {
        return 1;
    }

private:
    T* typedPtr_; ///< Pointer to store the parsed value.
};

template <>
class TypedValueHandler<std::string> : public OptionValueHandler
{
public:
    TypedValueHandler(std::string* typedPtr = nullptr)
        : typedPtr_(typedPtr)
    {
    }

    void parse(nonstd::any& value, nonstd::span<const nonstd::string_view> args) override
    {
        if (args.size() < minTokens())
        {
            ThrowIfTrue(true, "Expected at least {} value(s), got {}", minTokens(), args.size());
        }

        if (args.size() > maxTokens())
        {
            ThrowIfTrue(true, "Expected at most {} value(s), got {}", maxTokens(), args.size());
        }

        // Оптимизация: один проход с reserve
        if (args.empty())
        {
            value = std::string();
            return;
        }

        if (args.size() == 1)
        {
            // Один токен - просто копируем без лишних операций
            value = std::string(args[0]);
            return;
        }

        // Много токенов - объединяем за один проход
        std::string result;

        // Первый токен (без пробела перед ним)
        result.reserve(args.size() * 16); // Примерная оценка
        result += args[0];

        // Остальные токены с пробелом
        for (size_t i = 1; i < args.size(); ++i)
        {
            result += ' ';
            result += args[i];
        }

        value = std::move(result);
    }

    void notify(const nonstd::any& valueStore) const override
    {
        const std::string* value = nonstd::any_cast<std::string>(&valueStore);
        if (typedPtr_ && value)
        {
            *typedPtr_ = *value;
        }
    }

    std::size_t minTokens() const override
    {
        return 0;
    }
    std::size_t maxTokens() const override
    {
        return std::numeric_limits<std::size_t>::max();
    }

private:
    std::string* typedPtr_;
};

/// @brief Factory function for creating a TypedValueHandler without binding.
///
/// @tparam T The type of value to handle.
/// @return Shared pointer to the created handler.
template <class T>
std::shared_ptr<TypedValueHandler<T>> Value()
{
    return std::make_shared<TypedValueHandler<T>>(nullptr);
}

/// @brief Factory function for creating a TypedValueHandler with binding.
///
/// @tparam T The type of value to handle.
/// @param v Pointer to store the parsed value.
/// @return Shared pointer to the created handler.
template <class T>
std::shared_ptr<TypedValueHandler<T>> Value(T* v)
{
    return std::make_shared<TypedValueHandler<T>>(v);
}

} // namespace casket::opt