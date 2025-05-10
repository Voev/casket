#pragma once
#include <cassert>
#include <memory>
#include <type_traits>
#include <casket/opt/option.hpp>

namespace casket::opt
{

class OptionBuilder
{
public:
    explicit OptionBuilder(std::string name)
        : option_(std::move(name))
    {
    }

    OptionBuilder(std::string name, std::shared_ptr<OptionValueHandler> valueHandler)
        : option_(std::move(name), valueHandler)
    {
    }

    OptionBuilder& setRequired()
    {
        option_.isRequired_ = true;
        return *this;
    }

    OptionBuilder& setDescription(std::string desc)
    {
        option_.description_ = std::move(desc);
        return *this;
    }

    template <typename T>
    OptionBuilder& setDefaultValue(T&& value)
    {
        if constexpr (std::is_convertible_v<std::decay_t<T>, std::string_view>)
        {
            option_.defaultValue_ = std::string(std::forward<T>(value));
        }
        else
        {
            option_.defaultValue_ = std::forward<T>(value);
        }
        return *this;
    }

    Option build()
    {
        return std::move(option_);
    }

private:
    Option option_;
};

} // namespace casket::opt