#pragma once
#include <iosfwd>
#include <string>

#include <casket/opt/config_options.hpp>
#include <casket/utils/noncopyable.hpp>
#include <casket/utils/string.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

class ConfigOptionsReader final : public NonCopyable
{
public:
    ConfigOptionsReader()
    {
    }

    ~ConfigOptionsReader() noexcept
    {
    }

    void read(std::istream& is, ConfigOptions& config)
    {
        std::vector<std::string> sectionStack;
        std::vector<std::string> sectionData;
        std::string line;
        std::size_t lineno{0};

        while (getNextLine(is, line, lineno))
        {
            if (line.back() == '{')
            {
                if (sectionStack.empty())
                {
                    line.pop_back();
                    rtrim(line);
                }
                else
                {
                    sectionData.push_back(line);
                }
                sectionStack.push_back(line);
            }
            else if (equals(line, "}"))
            {
                ThrowIfTrue(sectionStack.empty(), "[Line {}] Mismatched closing brace", lineno);
                std::string completedSection = sectionStack.back();
                sectionStack.pop_back();

                if (sectionStack.empty())
                {
                    handle(config, completedSection, sectionData);
                    sectionData.clear();
                }
                else
                {
                    sectionData.push_back("}");
                }
            }
            else if (!sectionStack.empty())
            {
                sectionData.push_back(line);
            }
        }

        ThrowIfFalse(sectionStack.empty(), "[Section '{}'] Missing closing brace", sectionStack.back());
    }

private:
    static inline bool getNextLine(std::istream& is, std::string& line, std::size_t& lineno)
    {
        while (std::getline(is, line))
        {
            lineno++;

            ltrim(line);

            auto commentPos = line.find_first_of('#');
            if (commentPos != std::string::npos)
            {
                line.erase(commentPos);
            }

            rtrim(line);

            if (line.empty())
            {
                continue;
            }

            return true;
        }

        return false;
    }

    void handle(const ConfigOptions& config, const std::string& name, const std::vector<std::string>& lines)
    {
        Section* section = config.find(name);
        ThrowIfTrue(section == nullptr, "Unknown section: {}", name);
        try
        {
            section->parse(lines);
            section->validate();
        }
        catch (std::exception& e)
        {
            throw RuntimeError("[Section '{}'] {}", name, e.what());
        }
    }
};

} // namespace casket::opt