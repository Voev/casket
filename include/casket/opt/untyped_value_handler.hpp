/// @file
/// @brief Untyped option value handler class definition.

#pragma once
#include <casket/opt/option_value_handler.hpp>

namespace casket::opt
{

class UntypedValueHandler : public OptionValueHandler
{
public:
    void parse(nonstd::any&, nonstd::span<const nonstd::string_view>) override
    {
        // Nothing to parse - untyped handler ignores all arguments
    }

    void notify(const nonstd::any&) const override
    {
        // Nothing to notify - no stored value
    }

    std::size_t minTokens() const override
    {
        return 0;
    }

    std::size_t maxTokens() const override
    {
        return 0;
    }
};

} // namespace casket::opt