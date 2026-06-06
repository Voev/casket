/// @file
/// @brief Untyped option value handler class definition.

#pragma once
#include <casket/opt/option_value_handler.hpp>

namespace casket::opt
{

/// @brief Untyped option value handler.
class UntypedValueHandler : public OptionValueHandler
{
public:
    /// @brief No-op parse implementation.
    ///
    /// This implementation does nothing as the value is untyped and doesn't
    /// require any parsing.
    ///
    /// @param value Reference to `std::any` where the result would normally be
    /// stored.
    /// @param args Span of string_view arguments intended for parsing.
    void parse(nonstd::any&, nonstd::span<const nonstd::string_view>) override
    {
        // Nothing to parse - untyped handler ignores all arguments
    }

    /// @brief No-op notify implementation.
    ///
    /// This method does nothing since there is no actual value to notify about.
    ///
    /// @param valueStore The value that was supposedly set, unused.
    void notify(const nonstd::any&) const override
    {
        // Nothing to notify - no stored value
    }

    /// @brief Returns 0 as the minimum number of tokens.
    ///
    /// Since the value is untyped, no tokens are required to represent its
    /// value.
    ///
    /// @return Always returns 0.
    std::size_t minTokens() const override
    {
        return 0;
    }

    /// @brief Returns 0 as the maximum number of tokens.
    ///
    /// As with minimum tokens, no tokens can or need to be consumed for
    /// representing this value.
    ///
    /// @return Always returns 0.
    std::size_t maxTokens() const override
    {
        return 0;
    }
};

} // namespace casket::opt