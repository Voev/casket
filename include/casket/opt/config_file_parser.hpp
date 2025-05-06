#pragma once
#include <string>
#include <fstream>
#include <filesystem>

#include <casket/opt/config_options_reader.hpp>

#include <casket/utils/noncopyable.hpp>
#include <casket/utils/exception.hpp>

namespace casket::opt
{

class ConfigFileParser final : public utils::NonCopyable
{
public:
    explicit ConfigFileParser(const std::string& filename)
        : path_(filename)
    {
        utils::ThrowIfFalse(std::filesystem::exists(path_), "File not found: {}", filename);
        utils::ThrowIfFalse(std::filesystem::is_regular_file(path_), "Not a file: {}", filename);
    }

    ~ConfigFileParser() noexcept
    {
    }

    void parse(ConfigOptions& config)
    {
        ConfigOptionsReader reader;
        try
        {
            std::ifstream fstream(path_);
            reader.read(fstream, config);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(utils::format("[Config error: {}] {}", path_.filename().string(), e.what()));
        }
    }

private:
    std::filesystem::path path_;
};

} // namespace casket::opt
