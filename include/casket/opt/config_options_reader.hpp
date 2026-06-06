#pragma once
#include <iosfwd>
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>
#include <fstream>
#include <memory>

#include <casket/opt/config_options.hpp>
#include <casket/opt/option_builder.hpp>
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
        enableComments_ = true;
        enableMultilineStrings_ = true;
        enableVariables_ = true;
        enableIncludes_ = true;
        convertEqualsToSpace_ = true;
    }

    ~ConfigOptionsReader() noexcept = default;

    void read(std::istream& is, ConfigOptions& config)
    {
        readInternal(is, config, "");
    }

    void readFile(const std::string& filename, ConfigOptions& config)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            throw RuntimeError("Cannot open config file: {}", filename);
        }
        readInternal(file, config, filename);
    }

    void setEnableComments(bool enable)
    {
        enableComments_ = enable;
    }
    void setEnableMultilineStrings(bool enable)
    {
        enableMultilineStrings_ = enable;
    }
    void setEnableVariables(bool enable)
    {
        enableVariables_ = enable;
    }
    void setEnableIncludes(bool enable)
    {
        enableIncludes_ = enable;
    }
    void setConvertEqualsToSpace(bool convert)
    {
        convertEqualsToSpace_ = convert;
    }

    void setIncludePaths(const std::vector<std::string>& paths)
    {
        includePaths_ = paths;
    }
    void addIncludePath(const std::string& path)
    {
        includePaths_.push_back(path);
    }

private:
    struct ParserContext
    {
        ConfigOptions* config;
        std::vector<std::string> sectionStack;
        std::vector<std::vector<std::string>> sectionDataStack; // Stack of section data
        std::unordered_map<std::string, std::string> localVars;
        std::unordered_map<std::string, std::string> globalVars;
        std::string currentFile;
        std::size_t lineno;
        bool inMultiline;
        std::string multilineDelimiter;
        std::string multilineBuffer;
        std::string multilineKey;
        bool skipCurrentBlock;
        int skipBraceCount;

        ParserContext(ConfigOptions* cfg)
            : config(cfg)
            , lineno(0)
            , inMultiline(false)
            , skipCurrentBlock(false)
            , skipBraceCount(0)
        {
            sectionDataStack.emplace_back(); // Initialize with empty data for root context
        }
    };

    void readInternal(std::istream& is, ConfigOptions& config, const std::string& sourceName)
    {
        ParserContext ctx(&config);
        ctx.currentFile = sourceName;
        std::string line;

        while (getNextLineEx(is, line, ctx))
        {
            if (ctx.skipCurrentBlock)
            {
                processSkippedLine(line, ctx);
                continue;
            }

            if (enableMultilineStrings_ && ctx.inMultiline)
            {
                processMultilineLine(line, ctx);
                continue;
            }

            if (enableComments_)
            {
                removeComments(line);
                if (line.empty())
                    continue;
            }

            if (enableIncludes_ && startsWith(line, "include"))
            {
                processInclude(line, ctx);
                continue;
            }

            if (enableVariables_ && startsWith(line, "$"))
            {
                processVariableDefinition(line, ctx);
                continue;
            }

            if (!line.empty() && line.back() == '{')
            {
                // Start of a section
                processSectionOpen(line, ctx);
            }
            else if (equals(line, "}"))
            {
                // End of a section
                processSectionClose(ctx);
                // Don't return here - continue parsing after closing brace
            }
            else if (!ctx.sectionStack.empty())
            {
                // Regular option line inside current section
                std::string processedLine = line;
                if (enableVariables_)
                {
                    processedLine = expandVariables(line, ctx);
                }

                if (convertEqualsToSpace_)
                {
                    processedLine = normalizeOptionLine(processedLine);
                }

                // Add to current section's data (top of stack)
                if (!ctx.sectionDataStack.empty())
                {
                    ctx.sectionDataStack.back().push_back(processedLine);
                }
            }
        }

        // Check for unclosed sections at EOF
        if (!ctx.sectionStack.empty())
        {
            ThrowIfFalse(ctx.sectionStack.empty(), "[Section '{}'] Missing closing brace", ctx.sectionStack.back());
        }
    }

    void processSkippedLine(const std::string& line, ParserContext& ctx)
    {
        for (char c : line)
        {
            if (c == '{')
                ctx.skipBraceCount++;
            else if (c == '}')
            {
                ctx.skipBraceCount--;
                if (ctx.skipBraceCount == 0)
                {
                    ctx.skipCurrentBlock = false;
                }
            }
        }
    }

    void processMultilineLine(std::string& line, ParserContext& ctx)
    {
        size_t pos = line.find(ctx.multilineDelimiter);
        if (pos != std::string::npos)
        {
            // End of multiline string
            ctx.multilineBuffer += line.substr(0, pos);
            ctx.inMultiline = false;

            if (!ctx.sectionStack.empty() && !ctx.sectionDataStack.empty())
            {
                std::string multilineValue = ctx.multilineBuffer;
                trim(multilineValue);

                // Store as key + value pair
                std::string optionLine = ctx.multilineKey + " " + multilineValue;
                ctx.sectionDataStack.back().push_back(optionLine);
            }

            ctx.multilineBuffer.clear();
            ctx.multilineKey.clear();
        }
        else
        {
            // Continue collecting multiline content
            ctx.multilineBuffer += line + "\n";
        }
    }

    void processInclude(const std::string& line, ParserContext& ctx)
    {
        std::string includePath = extractIncludePath(line);
        includePath = expandVariables(includePath, ctx);

        std::string fullPath = findIncludeFile(includePath, ctx.currentFile);
        if (!fullPath.empty())
        {
            std::ifstream includeFile(fullPath);
            if (includeFile.is_open())
            {
                // Build current section prefix
                std::string sectionPrefix;
                for (const auto& s : ctx.sectionStack)
                {
                    if (!sectionPrefix.empty())
                        sectionPrefix += ".";
                    sectionPrefix += s;
                }

                readInternal(includeFile, *ctx.config, fullPath);
            }
            else
            {
                throw RuntimeError("Cannot open include file: {}", fullPath);
            }
        }
        else
        {
            throw RuntimeError("Include file not found: {}", includePath);
        }
    }

    std::string extractIncludePath(const std::string& line)
    {
        std::string path = line.substr(7); // after "include"
        trim(path);

        if ((path.front() == '"' && path.back() == '"') || (path.front() == '\'' && path.back() == '\''))
        {
            path = path.substr(1, path.length() - 2);
            trim(path);
        }

        return path;
    }

    void processVariableDefinition(const std::string& line, ParserContext& ctx)
    {
        std::string varLine = line.substr(1); // remove $
        trim(varLine);

        size_t eqPos = varLine.find('=');
        ThrowIfTrue(eqPos == std::string::npos, "Invalid variable definition: {}", line);

        std::string varName = varLine.substr(0, eqPos);
        trim(varName);

        std::string varValue = varLine.substr(eqPos + 1);
        trim(varValue);

        // Remove quotes if present
        if ((varValue.front() == '"' && varValue.back() == '"') ||
            (varValue.front() == '\'' && varValue.back() == '\''))
        {
            varValue = varValue.substr(1, varValue.length() - 2);
            trim(varValue);
        }

        varValue = expandVariables(varValue, ctx);

        if (ctx.sectionStack.empty())
        {
            ctx.globalVars[varName] = varValue;
        }
        else
        {
            ctx.localVars[varName] = varValue;
        }
    }

    void processSectionOpen(const std::string& line, ParserContext& ctx)
    {
        std::string sectionName = line;
        sectionName.pop_back(); // remove '{'
        trim(sectionName);
        sectionName = parseSectionName(sectionName, ctx);

        // Push section name to stack
        ctx.sectionStack.push_back(sectionName);

        // Push new empty data vector for this section
        ctx.sectionDataStack.emplace_back();
    }

    std::string parseSectionName(const std::string& sectionLine, ParserContext& ctx)
    {
        std::string result = sectionLine;

        // Remove quotes
        if ((result.front() == '"' && result.back() == '"') || (result.front() == '\'' && result.back() == '\''))
        {
            result = result.substr(1, result.length() - 2);
            trim(result);
        }

        result = expandVariables(result, ctx);
        return result;
    }

    void processSectionClose(ParserContext& ctx)
    {
        ThrowIfTrue(ctx.sectionStack.empty(), "Mismatched closing brace", ctx.lineno);

        // Get current section data before popping
        std::vector<std::string> currentSectionData = std::move(ctx.sectionDataStack.back());
        ctx.sectionDataStack.pop_back();

        std::string fullSectionName = ctx.sectionStack.back();
        ctx.sectionStack.pop_back();

        // Find the section in config
        Section* section = ctx.config->find(fullSectionName);

        // If not found with full name, try to find just the base name
        if (section == nullptr)
        {
            size_t dotPos = fullSectionName.find_last_of('.');
            std::string baseName = (dotPos != std::string::npos) ? fullSectionName.substr(dotPos + 1) : fullSectionName;
            section = ctx.config->find(baseName);

            if (section == nullptr)
            {
                section = ctx.config->find(fullSectionName);
            }
        }

        ThrowIfTrue(section == nullptr, "Unknown section: {}", fullSectionName);

        try
        {
            std::vector<nonstd::string_view> views;
            views.reserve(currentSectionData.size());
            for (const auto& line : currentSectionData)
            {
                views.push_back(line);
            }
            
            section->parse(nonstd::span<const nonstd::string_view>(views));
            section->validate();
        }
        catch (std::exception& e)
        {
            throw RuntimeError("[Section '{}'] {}", fullSectionName, e.what());
        }

        // Clear local variables when exiting section
        ctx.localVars.clear();
    }

    bool getNextLineEx(std::istream& is, std::string& line, ParserContext& ctx)
    {
        while (std::getline(is, line))
        {
            ctx.lineno++;

            trim(line); // ltrim + rtrim

            if (enableMultilineStrings_ && !ctx.inMultiline)
            {
                checkMultilineStart(line, ctx);
                if (ctx.inMultiline)
                {
                    continue;
                }
            }

            if (enableComments_ && !ctx.inMultiline)
            {
                removeComments(line);
            }

            if (line.empty() && !ctx.inMultiline)
            {
                continue;
            }

            return true;
        }

        return false;
    }

    void checkMultilineStart(const std::string& line, ParserContext& ctx)
    {
        // Look for pattern: key = """ or key = '''
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos)
        {
            return;
        }

        // Find start of multiline delimiter after equals sign
        size_t delimiterPos = line.find("\"\"\"", eqPos);
        if (delimiterPos != std::string::npos)
        {
            // Extract key name
            std::string key = line.substr(0, eqPos);
            trim(key);
            ctx.multilineKey = key;

            ctx.inMultiline = true;
            ctx.multilineDelimiter = "\"\"\"";

            // Get content after delimiter
            std::string after = line.substr(delimiterPos + 3);
            trim(after);

            // Check if multiline ends on same line (empty multiline)
            if (after.find("\"\"\"") != std::string::npos)
            {
                // Empty multiline string
                ctx.inMultiline = false;
                if (!ctx.sectionDataStack.empty())
                {
                    ctx.sectionDataStack.back().push_back(key + " \"\"");
                }
                ctx.multilineKey.clear();
            }
            else if (!after.empty())
            {
                ctx.multilineBuffer = after;
            }
            return;
        }

        delimiterPos = line.find("'''", eqPos);
        if (delimiterPos != std::string::npos)
        {
            // Extract key name
            std::string key = line.substr(0, eqPos);
            trim(key);
            ctx.multilineKey = key;

            ctx.inMultiline = true;
            ctx.multilineDelimiter = "'''";

            // Get content after delimiter
            std::string after = line.substr(delimiterPos + 3);
            trim(after);

            if (after.find("'''") != std::string::npos)
            {
                // Empty multiline string
                ctx.inMultiline = false;
                if (!ctx.sectionDataStack.empty())
                {
                    ctx.sectionDataStack.back().push_back(key + " \"\"");
                }
                ctx.multilineKey.clear();
            }
            else if (!after.empty())
            {
                ctx.multilineBuffer = after;
            }
            return;
        }
    }

    void removeComments(std::string& line)
    {
        bool inQuotes = false;
        char quoteChar = '\0';

        for (size_t i = 0; i < line.length(); ++i)
        {
            if (!inQuotes && (line[i] == '"' || line[i] == '\''))
            {
                inQuotes = true;
                quoteChar = line[i];
            }
            else if (inQuotes && line[i] == quoteChar)
            {
                inQuotes = false;
            }
            else if (!inQuotes && line[i] == '#')
            {
                line = line.substr(0, i);
                trim(line);
                break;
            }
            else if (!inQuotes && line[i] == '/' && i + 1 < line.length() && line[i + 1] == '/')
            {
                line = line.substr(0, i);
                trim(line);
                break;
            }
        }
    }

    std::string normalizeOptionLine(const std::string& line)
    {
        // Skip lines that look like section starts (they contain '{')
        if (!line.empty() && line.back() == '{')
        {
            return line;
        }

        // Find equals sign not in quotes
        bool inQuotes = false;
        char quoteChar = '\0';
        size_t equalsPos = std::string::npos;

        for (size_t i = 0; i < line.length(); ++i)
        {
            if (!inQuotes && (line[i] == '"' || line[i] == '\''))
            {
                inQuotes = true;
                quoteChar = line[i];
            }
            else if (inQuotes && line[i] == quoteChar)
            {
                inQuotes = false;
            }
            else if (!inQuotes && line[i] == '=')
            {
                equalsPos = i;
                break;
            }
        }

        if (equalsPos != std::string::npos)
        {
            // Convert "key = value" to "key value"
            std::string key = line.substr(0, equalsPos);
            trim(key);

            std::string value = line.substr(equalsPos + 1);
            trim(value);

            // Remove quotes if present
            if ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))
            {
                value = value.substr(1, value.length() - 2);
                trim(value);
            }

            return key + " " + value;
        }

        return line;
    }

    std::string expandVariables(const std::string& input, ParserContext& ctx)
    {
        std::string result;
        bool inVar = false;
        bool inBraces = false;
        std::string varName;

        for (size_t i = 0; i < input.length(); ++i)
        {
            if (!inVar && input[i] == '$' && i + 1 < input.length())
            {
                inVar = true;
                continue;
            }

            if (inVar && !inBraces && input[i] == '{')
            {
                inBraces = true;
                continue;
            }

            if (inVar && ((inBraces && input[i] == '}') ||
                          (!inBraces && !std::isalnum(static_cast<unsigned char>(input[i])) && input[i] != '_')))
            {
                // End of variable
                std::string value = getVariableValue(varName, ctx);
                result += value;

                inVar = false;
                inBraces = false;
                varName.clear();

                if (input[i] != '}')
                {
                    result += input[i];
                }
                continue;
            }

            if (inVar)
            {
                varName += input[i];
            }
            else
            {
                result += input[i];
            }
        }

        if (inVar && !varName.empty())
        {
            result += getVariableValue(varName, ctx);
        }

        return result;
    }

    std::string getVariableValue(const std::string& varName, ParserContext& ctx)
    {
        // Check local variables
        auto it = ctx.localVars.find(varName);
        if (it != ctx.localVars.end())
        {
            return it->second;
        }

        // Check global variables
        it = ctx.globalVars.find(varName);
        if (it != ctx.globalVars.end())
        {
            return it->second;
        }

        // Environment variable
        const char* envValue = std::getenv(varName.c_str());
        if (envValue)
        {
            return std::string(envValue);
        }

        return "";
    }

    std::string findIncludeFile(const std::string& filename, const std::string& currentFile)
    {
        // Check relative to current file
        if (!currentFile.empty())
        {
            size_t lastSlash = currentFile.find_last_of("/\\");
            if (lastSlash != std::string::npos)
            {
                std::string currentDir = currentFile.substr(0, lastSlash);
                std::string fullPath = currentDir + "/" + filename;

                std::ifstream test(fullPath);
                if (test.is_open())
                {
                    return fullPath;
                }
            }
        }

        // Check absolute path
        std::ifstream test(filename);
        if (test.is_open())
        {
            return filename;
        }

        // Search in include paths
        for (const auto& path : includePaths_)
        {
            std::string fullPath = path;
            if (!fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\')
            {
                fullPath += "/";
            }
            fullPath += filename;

            std::ifstream test2(fullPath);
            if (test2.is_open())
            {
                return fullPath;
            }
        }

        return "";
    }

    bool startsWith(const std::string& str, const std::string& prefix)
    {
        return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
    }

private:
    bool enableComments_{true};
    bool enableMultilineStrings_{true};
    bool enableVariables_{true};
    bool enableIncludes_{true};
    bool convertEqualsToSpace_{true};
    std::vector<std::string> includePaths_;
};

} // namespace casket::opt