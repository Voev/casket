#pragma once
#include <vector>
#include <casket/nonstd/span.hpp>
#include <casket/nonstd/string_view.hpp>

namespace casket::opt
{

class SpanBuilder
{
public:
    static inline auto make(const nonstd::string_view& arg)
    {
        return nonstd::span<const nonstd::string_view>(&arg, 1);
    }

    static inline auto make(const std::vector<std::string>& args)
    {
        std::vector<nonstd::string_view> views;
        views.reserve(args.size());
        for (const auto& arg : args)
        {
            views.push_back(arg);
        }
        return views;
    }
};

} // namespace casket::opt