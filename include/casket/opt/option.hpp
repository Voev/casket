#pragma once
#include <string>
#include <memory>
#include <optional>
#include <any>
#include <vector>
#include <sstream>

#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

#include <casket/nonstd/optional.hpp>

#include <casket/opt/option_value_handler.hpp>
#include <casket/opt/untyped_value_handler.hpp>

namespace casket::opt
{

class OptionBuilder;

class Option final
{
    friend class OptionBuilder;

public:
    Option(std::string name)
        : Option(std::initializer_list<std::string>{std::move(name)}, nullptr)
    {
    }

    Option(std::string name, std::shared_ptr<OptionValueHandler> valueSemantic)
        : Option(std::initializer_list<std::string>{std::move(name)}, std::move(valueSemantic))
    {
    }

    Option(std::initializer_list<std::string> names)
        : Option(names, nullptr)
    {
    }

    Option(std::initializer_list<std::string> names, std::shared_ptr<OptionValueHandler> valueSemantic)
        : valueHandler_(valueSemantic ? std::move(valueSemantic) : std::make_shared<UntypedValueHandler>())

        , isRequired_(false)
        , isUsed_(false)
    {
        ThrowIfTrue(names.size() == 0, "Empty option name list");

        for (const auto& n : names)
        {
            ThrowIfTrue(n.empty(), "Empty option name");

            names_.push_back(n);
        }

        for (std::size_t i = 0; i < names_.size(); ++i)
            for (std::size_t j = i + 1; j < names_.size(); ++j)
                ThrowIfTrue(names_[i] == names_[j], "Duplicate option name '{}'", names_[i]);
    }

    ~Option() noexcept
    {
    }

    Option(const Option& other)
        : names_(other.names_)
        , description_(other.description_)
        , defaultValue_(other.defaultValue_)
        , value_(other.value_)
        , valueHandler_(other.valueHandler_)
        , isRequired_(other.isRequired_)
        , isUsed_(other.isUsed_)
    {
    }

    Option(Option&& other) noexcept
        : names_(std::move(other.names_))
        , description_(std::move(other.description_))
        , defaultValue_(std::move(other.defaultValue_))
        , value_(std::move(other.value_))
        , valueHandler_(std::move(other.valueHandler_))
        , isRequired_(other.isRequired_)
        , isUsed_(other.isUsed_)
    {
    }

    Option& operator=(const Option& other)
    {
        if (this != &other)
        {
            names_ = other.names_;
            description_ = other.description_;
            defaultValue_ = other.defaultValue_;
            value_ = other.value_;
            valueHandler_ = other.valueHandler_;
            isRequired_ = other.isRequired_;
            isUsed_ = other.isUsed_;
        }
        return *this;
    }

    Option& operator=(Option&& other) noexcept
    {
        if (this != &other)
        {
            names_ = std::move(other.names_);
            description_ = std::move(other.description_);
            defaultValue_ = std::move(other.defaultValue_);
            value_ = std::move(other.value_);
            valueHandler_ = std::move(other.valueHandler_);
            isRequired_ = other.isRequired_;
            isUsed_ = other.isUsed_;
        }
        return *this;
    }

    bool isRequired() const noexcept
    {
        return isRequired_;
    }

    bool isUsed() const noexcept
    {
        return isUsed_;
    }

    const std::string& getName() const noexcept
    {
        return names_.front();
    }

    const std::vector<std::string>& getNames() const noexcept
    {
        return names_;
    }

    const std::string& getDescription() const noexcept
    {
        return description_;
    }

    void consume(nonstd::span<const nonstd::string_view> arg)
    {
        ThrowIfTrue(isUsed_, "{}: option has already been processed", getName());
        valueHandler_->parse(value_, arg);
        isUsed_ = true;
    }

    template <typename T>
    T get() const
    {
        ThrowIfFalse(value_.has_value(), "{}: no value provided", getName());
        return nonstd::any_cast<T>(value_);
    }

    template <typename T>
    nonstd::optional<T> present() const
    {
        if (!value_.has_value())
        {
            return std::nullopt;
        }
        return nonstd::any_cast<T>(value_);
    }

    void validate()
    {
        if (!value_.has_value() && defaultValue_.has_value())
        {
            value_ = defaultValue_;
        }

        if (isRequired_)
        {
            ThrowIfTrue(!isUsed_ && !value_.has_value(), "{}: option is required but not provided", getName());
        }

        if (valueHandler_)
        {
            valueHandler_->notify(value_);
        }
    }

    std::size_t maxTokens() const noexcept
    {
        return valueHandler_->maxTokens();
    }

    std::size_t minTokens() const noexcept
    {
        return valueHandler_->minTokens();
    }

private:
    std::vector<std::string> names_;
    std::string description_;
    nonstd::any defaultValue_;
    nonstd::any value_;
    std::shared_ptr<OptionValueHandler> valueHandler_;
    bool isRequired_;
    bool isUsed_;
};

} // namespace casket::opt
