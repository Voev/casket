/// @file
/// @brief Typed option value handler class definition.

#pragma once
#include <memory>
#include <sstream>
#include <casket/opt/option_value_handler.hpp>
#include <casket/utils/string.hpp>

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
    TypedValueHandler(T* typedPtr)
        : typedPtr_(typedPtr)
    {
    }

    /// @brief Parses a string argument into a value of type `T`.
    ///
    /// This method attempts to convert the first string argument into a value
    /// of type `T`. If successful, it stores the result in an `std::any`
    /// object. Throws an exception if parsing fails.
    ///
    /// @param value    The `std::any` object where the parsed value is stored.
    /// @param args     A list of string arguments; the first argument is
    /// parsed.
    ///
    /// @throws std::runtime_error If the string cannot be parsed into type `T`.
    void parse(std::any& value, const std::vector<std::string_view>& args) override
    {
        auto str = std::string(args.front());
        std::istringstream iss{str};
        T typedValue{};

        using namespace casket::utils;

        if constexpr (std::is_same_v<T, bool>)
        {
            if (iequals(str, "true") || iequals(str, "yes"))
            {
                typedValue = true;
            }
            else if (iequals(str, "false") || iequals(str, "no"))
            {
                typedValue = false;
            }
            else
            {
                throw std::runtime_error("could not parse bool value: " + str);
            }
            value = std::move(typedValue);
        }
        else
        {
            if (iss >> typedValue)
            {
                value = std::move(typedValue);
            }
            else
            {
                throw std::runtime_error("could not parse value: " + str);
            }
        }
    }

    /// @brief Notification callback for when the value is set.
    ///
    /// This method ensures that if a value is successfully parsed, it is
    /// stored in the provided pointer to hold the typed value. If the pointer
    /// is null or the value is of a wrong type, no action is taken.
    ///
    /// @param valueStore The `std::any` containing the parsed value of type
    /// `T`.
    void notify(const std::any& valueStore) const override
    {
        const T* value = std::any_cast<T>(&valueStore);
        if (typedPtr_ && value)
        {
            *typedPtr_ = *value;
        }
    }

    /// @brief Returns the minimum number of tokens required for parsing.
    ///
    /// For `TypedValue`, one token is always required as an input.
    ///
    /// @return Always returns 1.
    std::size_t minTokens() const override
    {
        return 1U;
    }

    /// @brief Returns the maximum number of tokens allowed for parsing.
    ///
    /// For `TypedValue`, one token is the maximum allowed since it parses
    /// a single argument into type `T`.
    ///
    /// @return Always returns 1.
    std::size_t maxTokens() const override
    {
        return 1U;
    }

private:
    T* typedPtr_; ///< Pointer to store the parsed value.
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