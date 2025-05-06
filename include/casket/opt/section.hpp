#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <casket/opt/option.hpp>

namespace casket::opt
{

class Section
{
public:
    virtual ~Section() = default;

    virtual void parse(const std::vector<std::string>& lines)
    {
        for (const auto& line : lines)
        {
            auto args = utils::split(line, " ");
            auto option = options_.find(args[0]);
            utils::ThrowIfTrue(option == options_.end(), "Unknown option: {}", args[0]);
            option->second.consume(args.begin() + 1, args.end());
        }
        validate();
    }

    void addOption(Option&& option)
    {
        auto result = options_.emplace(option.getName(), std::move(option));
        utils::ThrowIfFalse(result.second, "Option '{}' already exists", result.first->first);
    }

    const Option& getOption(const std::string& name) const
    {
        auto it = options_.find(name);
        utils::ThrowIfTrue(it == options_.end(), "Option '{}' not found", name);
        return it->second;
    }

protected:
    void validate()
    {
        for (auto& [_, option] : options_)
        {
            option.validate();
        }
    }

private:
    std::unordered_map<std::string, Option> options_;
};

} // namespace casket::opt
