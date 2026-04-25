#pragma once
#include <vector>
#include <memory>
#include <sstream>
#include <casket/opt/option_value_handler.hpp>
#include <casket/opt/value_parser.hpp>
#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

template <typename T>
class MultiOptionValueHandler : public OptionValueHandler
{
public:
    MultiOptionValueHandler(std::vector<T>* vecPtr)
        : vecPtr_(vecPtr)
    {
    }

    void parse(std::any& value, const std::vector<std::string>& args) override
    {
        ThrowIfTrue(args.size() < minTokens(), "not enough arguments");

        std::vector<T> values;
        values.reserve(args.size());

        for (const auto& arg : args)
        {
            T parsedValue = ValueParser<T>::parse(arg);
            values.push_back(std::move(parsedValue));
        }

        value = std::move(values);
    }

    void notify(const std::any& valueStore) const override
    {
        const std::vector<T>* values = std::any_cast<std::vector<T>>(&valueStore);
        if (vecPtr_ && values)
        {
            *vecPtr_ = *values;
        }
    }

    std::size_t minTokens() const override
    {
        return 1;
    }

    std::size_t maxTokens() const override
    {
        return std::numeric_limits<std::size_t>::max();
    }

private:
    std::vector<T>* vecPtr_;
};

// Фабричные функции
template <class T>
std::shared_ptr<MultiOptionValueHandler<T>> MultiValue()
{
    return std::make_shared<MultiOptionValueHandler<T>>(nullptr);
}

template <class T>
std::shared_ptr<MultiOptionValueHandler<T>> MultiValue(std::vector<T>* v)
{
    return std::make_shared<MultiOptionValueHandler<T>>(v);
}

} // namespace casket::opt