#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <casket/nonstd/span.hpp>
#include <casket/nonstd/string_view.hpp>
#include <casket/opt/option.hpp>

namespace casket::opt
{

class Section
{
public:
    virtual ~Section() = default;

    virtual void parse(nonstd::span<const nonstd::string_view> lines)
    {
        for (const auto& line : lines)
        {
            auto tokens = splitToViewsWithQuotes(line);
            if (tokens.empty())
                continue;

            std::string key(tokens[0]);

            auto option = options_.find(key);
            ThrowIfTrue(option == options_.end(), "Unknown option: {}", key);

            if (tokens.size() > 1)
            {
                nonstd::span<const nonstd::string_view> values(tokens.data() + 1, tokens.size() - 1);
                option->second.consume(values);
            }
            else
            {
                // No values
                option->second.consume({});
            }
        }
    }

    void validate()
    {
        for (auto& [_, option] : options_)
        {
            option.validate();
        }
    }

    void addOption(Option&& option)
    {
        auto result = options_.emplace(option.getName(), std::move(option));
        ThrowIfFalse(result.second, "Option '{}' already exists", result.first->first);
    }

    const Option& getOption(const std::string& name) const
    {
        auto it = options_.find(name);
        ThrowIfTrue(it == options_.end(), "Option '{}' not found", name);
        return it->second;
    }

protected:
    std::vector<nonstd::string_view> splitToViewsWithQuotes(nonstd::string_view line)
    {
        std::vector<nonstd::string_view> tokens;

        size_t i = 0;
        while (i < line.length())
        {
            // Skip spaces
            while (i < line.length() && std::isspace(static_cast<unsigned char>(line[i])))
            {
                ++i;
            }

            if (i >= line.length())
                break;

            // Handle array syntax: [value1, value2, value3]
            if (line[i] == '[')
            {
                i++; // Skip '['

                // Parse array contents
                while (i < line.length() && line[i] != ']')
                {
                    // Skip spaces
                    while (i < line.length() && std::isspace(static_cast<unsigned char>(line[i])))
                    {
                        ++i;
                    }

                    if (i >= line.length() || line[i] == ']')
                        break;

                    // Handle quoted values inside array
                    if (line[i] == '"' || line[i] == '\'')
                    {
                        char quoteChar = line[i];
                        size_t start = i + 1;
                        size_t end = start;

                        while (end < line.length() && line[end] != quoteChar)
                        {
                            ++end;
                        }

                        tokens.push_back(line.substr(start, end - start));
                        i = end + 1;
                    }
                    else
                    {
                        // Regular value without quotes
                        size_t start = i;
                        while (i < line.length() && line[i] != ',' && line[i] != ']' &&
                               !std::isspace(static_cast<unsigned char>(line[i])))
                        {
                            ++i;
                        }
                        tokens.push_back(line.substr(start, i - start));
                    }

                    // Skip comma
                    while (i < line.length() && (line[i] == ',' || std::isspace(static_cast<unsigned char>(line[i]))))
                    {
                        ++i;
                    }
                }

                i++; // Skip ']'
                continue;
            }

            // Handle quoted token
            if (line[i] == '"' || line[i] == '\'')
            {
                char quoteChar = line[i];
                size_t start = i + 1;
                size_t end = start;

                while (end < line.length() && line[end] != quoteChar)
                {
                    ++end;
                }

                tokens.push_back(line.substr(start, end - start));
                i = end + 1;
            }
            else
            {
                // Regular token
                size_t start = i;
                while (i < line.length() && !std::isspace(static_cast<unsigned char>(line[i])))
                {
                    ++i;
                }
                tokens.push_back(line.substr(start, i - start));
            }
        }

        return tokens;
    }

private:
    std::unordered_map<std::string, Option> options_;
};

} // namespace casket::opt