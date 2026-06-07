#pragma once
#include <array>
#include <vector>
#include <string>
#include <casket/nonstd/any.hpp>
#include <casket/nonstd/string_view.hpp>
#include <casket/nonstd/span.hpp>

namespace casket::opt
{

class OptionValueHandler
{
public:
    virtual ~OptionValueHandler() = default;

    virtual void parse(nonstd::any& value, nonstd::span<const nonstd::string_view> arg) = 0;

    virtual void notify(const nonstd::any& valueStore) const = 0;

    virtual std::size_t minTokens() const = 0;

    virtual std::size_t maxTokens() const = 0;
};

} // namespace casket::opt