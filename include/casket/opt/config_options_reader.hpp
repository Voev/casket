#pragma once
#include <iosfwd>
#include <string>

#include <casket/opt/config_options.hpp>
#include <casket/utils/noncopyable.hpp>
#include <casket/utils/string.hpp>

namespace casket::opt
{

class ConfigOptionsReader final : public utils::NonCopyable
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
                    utils::rtrim(line);
                }
                else
                {
                    sectionData.push_back(line);
                }
                sectionStack.push_back(line);
            }
            else if (utils::equals(line, "}"))
            {
                utils::ThrowIfTrue(sectionStack.empty(), "[Line {}] Mismatched closing brace", lineno);
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

        utils::ThrowIfFalse(sectionStack.empty(), "[Section '{}'] Missing closing brace", sectionStack.back());
    }

private:
    static inline bool getNextLine(std::istream& is, std::string& line, std::size_t& lineno)
    {
        while (std::getline(is, line))
        {
            lineno++;

            utils::ltrim(line);

            auto commentPos = line.find_first_of('#');
            if (commentPos != std::string::npos)
            {
                line.erase(commentPos);
            }

            utils::rtrim(line);

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
        utils::ThrowIfTrue(section == nullptr, "Unknown section: {}", name);
        try
        {
            section->parse(lines);
        }
        catch (std::exception& e)
        {
            throw std::runtime_error(utils::format("[Section '{}'] {}", name, e.what()));
        }
    }
};

} // namespace casket::opt