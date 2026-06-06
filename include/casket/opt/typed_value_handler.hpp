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

template <typename T>
class TypedValueHandler : public OptionValueHandler
{
public:
    TypedValueHandler(T* typedPtr = nullptr)
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

    void notify(const nonstd::any& valueStore) const override
    {
        const T* value = nonstd::any_cast<T>(&valueStore);
        if (typedPtr_ && value)
        {
            *typedPtr_ = *value;
        }
    }

    std::size_t minTokens() const override
    {
        return 1;
    }

    std::size_t maxTokens() const override
    {
        return 1;
    }

private:
    T* typedPtr_;
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

        if (args.empty())
        {
            value = std::string();
            return;
        }

        if (args.size() == 1)
        {
            value = std::string(args[0]);
            return;
        }

        std::string result;

        result.reserve(args.size() * 16);
        result += args[0];

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

template <class T>
std::shared_ptr<TypedValueHandler<T>> Value()
{
    return std::make_shared<TypedValueHandler<T>>(nullptr);
}

template <class T>
std::shared_ptr<TypedValueHandler<T>> Value(T* v)
{
    return std::make_shared<TypedValueHandler<T>>(v);
}

} // namespace casket::opt