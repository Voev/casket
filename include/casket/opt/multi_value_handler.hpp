#pragma once
#include <vector>
#include <memory>
#include <sstream>
#include <casket/nonstd/span.hpp>
#include <casket/nonstd/string_view.hpp>
#include <casket/opt/value_parser.hpp>
#include <casket/opt/option_value_handler.hpp>
#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

template <typename T>
class MultiOptionValueHandler : public OptionValueHandler
{
public:
    MultiOptionValueHandler(std::vector<T>* vecPtr = nullptr, 
                            std::size_t minCount = 1, 
                            std::size_t maxCount = std::numeric_limits<std::size_t>::max())
        : vecPtr_(vecPtr)
        , minCount_(minCount)
        , maxCount_(maxCount)
    {
    }

    // New interface: parse from span of string_view
    void parse(nonstd::any& value, nonstd::span<const nonstd::string_view> args) override
    {
        // Validate count first
        if (args.size() < minCount_)
        {
            ThrowIfTrue(true, "Expected at least {} value(s), got {}", minCount_, args.size());
        }
        
        if (args.size() > maxCount_)
        {
            ThrowIfTrue(true, "Expected at most {} value(s), got {}", maxCount_, args.size());
        }

        std::vector<T> values;
        values.reserve(args.size());
        
        for (const auto& arg : args)
        {
            values.push_back(ValueParser<T>::parse(arg));
        }
        
        value = std::move(values);
    }

    void notify(const nonstd::any& valueStore) const override
    {
        const std::vector<T>* values = nonstd::any_cast<std::vector<T>>(&valueStore);
        if (vecPtr_ && values)
        {
            *vecPtr_ = *values;
        }
    }

    std::size_t minTokens() const override
    {
        return minCount_;
    }

    std::size_t maxTokens() const override
    {
        return maxCount_;
    }

    MultiOptionValueHandler<T>& min(std::size_t minCount)
    {
        minCount_ = minCount;
        return *this;
    }
    
    MultiOptionValueHandler<T>& max(std::size_t maxCount)
    {
        maxCount_ = maxCount;
        return *this;
    }
    
    MultiOptionValueHandler<T>& exactly(std::size_t count)
    {
        minCount_ = count;
        maxCount_ = count;
        return *this;
    }
    
    MultiOptionValueHandler<T>& range(std::size_t minCount, std::size_t maxCount)
    {
        minCount_ = minCount;
        maxCount_ = maxCount;
        return *this;
    }

private:
    void validateCount(std::size_t count) const
    {
        ThrowIfTrue(count < minCount_, 
                    "Expected at least {} value(s), got {}", minCount_, count);
        ThrowIfTrue(count > maxCount_, 
                    "Expected at most {} value(s), got {}", maxCount_, count);
    }

private:
    std::vector<T>* vecPtr_;
    std::size_t minCount_;
    std::size_t maxCount_;
};

// Factory functions
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

template <class T>
std::shared_ptr<MultiOptionValueHandler<T>> MultiValue(std::vector<T>* v, std::size_t minCount)
{
    return std::make_shared<MultiOptionValueHandler<T>>(v, minCount);
}

template <class T>
std::shared_ptr<MultiOptionValueHandler<T>> MultiValue(std::vector<T>* v, 
                                                        std::size_t minCount, 
                                                        std::size_t maxCount)
{
    return std::make_shared<MultiOptionValueHandler<T>>(v, minCount, maxCount);
}

} // namespace casket::opt