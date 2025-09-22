/// @file
/// @brief Содержит объявление класса SocketPathHolder - класса для управления путем к файлу Unix-сокета.
/// @copyright Copyright 2024-2025 RED Security. All Rights Reserved.

#pragma once
#include <string>
#include <filesystem>
#include <casket/utils/exception.hpp>

namespace casket
{

class SocketPathHolder
{
public:
    explicit SocketPathHolder(const std::string& path)
        : path_(path)
    {
        namespace fs = std::filesystem;
        if (fs::exists(path_))
        {
            ThrowIfFalse(fs::is_socket(path_), "File '{}' exists and is not a socket", path_.string());
            fs::remove(path_);
        }
        else
        {
            fs::create_directories(path_.parent_path());
        }
    }

    ~SocketPathHolder() noexcept
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    std::string toString() const
    {
        return path_.string();
    }

private:
    std::filesystem::path path_;
};

} // namespace casket