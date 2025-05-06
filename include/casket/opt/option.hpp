#pragma once
#include <string>
#include <memory>
#include <optional>
#include <any>
#include <vector>
#include <sstream>

#include <casket/opt/untyped_value_handler.hpp>
#include <casket/opt/typed_value_handler.hpp>

#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

class OptionBuilder;

class Option final
{
    friend class OptionBuilder;

public:
    Option(std::string name)
        : name_(std::move(name))
        , valueHandler_(std::make_shared<UntypedValueHandler>())
        , isRequired_(false)
        , isUsed_(false)
    {
        utils::ThrowIfTrue(name_.empty(), "Empty option name");
    }

    Option(std::string name, std::shared_ptr<OptionValueHandler> valueSemantic)
        : name_(std::move(name))
        , valueHandler_(valueSemantic)
        , isRequired_(false)
        , isUsed_(false)
    {
        utils::ThrowIfTrue(name_.empty(), "Empty option name");
    }

    ~Option() noexcept
    {
    }

    Option(const Option& other)
        : name_(other.name_)
        , description_(other.description_)
        , defaultValue_(other.defaultValue_)
        , value_(other.value_)
        , valueHandler_(other.valueHandler_)
        , isRequired_(other.isRequired_)
        , isUsed_(other.isUsed_)
    {
    }

    Option(Option&& other) noexcept
        : name_(std::move(other.name_))
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
            name_ = other.name_;
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
            name_ = std::move(other.name_);
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
        return name_;
    }

    const std::string& getDescription() const noexcept
    {
        return description_;
    }

    template <typename Iterator>
    void consume(Iterator start, Iterator end)
    {
        utils::ThrowIfTrue(isUsed_, "{}: option has already been processed", name_);

        std::size_t distance = std::distance(start, end);

        utils::ThrowIfTrue(distance < valueHandler_->minTokens(), "{}: not enough arguments", name_);
        utils::ThrowIfTrue(distance > valueHandler_->maxTokens(), "{}: too many arguments", name_);

        valueHandler_->parse(value_, std::vector<std::string_view>(start, end));
        isUsed_ = true;
    }

    template <typename T>
    T get() const
    {
        utils::ThrowIfFalse(value_.has_value(), "{}: no value provided", name_);
        return std::any_cast<T>(value_);
    }

    template <typename T>
    std::optional<T> present() const
    {
        if (!value_.has_value())
        {
            return std::nullopt;
        }
        return std::any_cast<T>(value_);
    }

    void validate()
    {
        if (!value_.has_value() && defaultValue_.has_value())
        {
            value_ = defaultValue_;
        }

        if (isRequired_)
        {
            utils::ThrowIfTrue(!isUsed_ && !value_.has_value(), "{}: option is required but not provided", name_);
        }

        if (valueHandler_)
        {
            valueHandler_->notify(value_);
        }
    }

    std::shared_ptr<OptionValueHandler> getValueHandler() const
    {
        return valueHandler_;
    }

private:
    std::string name_;
    std::string description_;
    std::any defaultValue_;
    std::any value_;
    std::shared_ptr<OptionValueHandler> valueHandler_;
    bool isRequired_;
    bool isUsed_;
};

} // namespace casket::opt
