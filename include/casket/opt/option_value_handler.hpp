#pragma once
#include <any>
#include <vector>
#include <string_view>

namespace casket::opt
{

class OptionValueHandler
{
public:
    virtual ~OptionValueHandler() = default;

    virtual void parse(std::any& value, const std::vector<std::string_view>& args) = 0;

    virtual void notify(const std::any& valueStore) const = 0;

    virtual std::size_t minTokens() const = 0;

    virtual std::size_t maxTokens() const = 0;
};

} // namespace casket::opt