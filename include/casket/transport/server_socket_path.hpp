#pragma once
#include <string>
#include <cstddef>
#include <filesystem>
#include <system_error>

namespace casket
{

class ServerSocketPath final
{
public:
    enum Flags : uint8_t
    {
        None = 0,
        OverwriteExisting = 1 << 0,
        CreateDirectories = 1 << 1,
        Default = OverwriteExisting | CreateDirectories
    };

    static ServerSocketPath create(std::string path, uint8_t flags, std::error_code& ec) noexcept
    {
        ServerSocketPath result;
        result.flags_ = flags;
        result.path_ = std::move(path);

        ec.clear();
        namespace fs = std::filesystem;

        bool exists = false;
        {
            std::error_code tmpEc;
            exists = fs::exists(result.path_, tmpEc);
            if (tmpEc)
            {
                ec = tmpEc;
                return ServerSocketPath();
            }
        }

        if (exists)
        {
            bool isSocket = false;
            {
                std::error_code tmpEc;
                isSocket = fs::is_socket(result.path_, tmpEc);
                if (tmpEc)
                {
                    ec = tmpEc;
                    return ServerSocketPath();
                }
            }

            if (!isSocket)
            {
                ec = std::make_error_code(std::errc::not_a_socket);
                return ServerSocketPath();
            }

            if (result.flags_ & OverwriteExisting)
            {
                std::error_code tmpEc;
                fs::remove(result.path_, tmpEc);
                if (tmpEc)
                {
                    ec = tmpEc;
                    return ServerSocketPath();
                }
            }
        }
        else if ((result.flags_ & CreateDirectories) && !result.path_.parent_path().empty())
        {
            std::error_code tmpEc;
            fs::create_directories(result.path_.parent_path(), tmpEc);
            if (tmpEc)
            {
                ec = tmpEc;
                return ServerSocketPath();
            }
        }

        return result;
    }

    ~ServerSocketPath() noexcept
    {
        if (flags_ & OverwriteExisting)
        {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
    }

    ServerSocketPath(const ServerSocketPath&) = delete;
    ServerSocketPath& operator=(const ServerSocketPath&) = delete;

    ServerSocketPath(ServerSocketPath&& other) noexcept
        : path_(std::move(other.path_))
        , flags_(std::exchange(other.flags_, 0))
    {
    }

    ServerSocketPath& operator=(ServerSocketPath&& other) noexcept
    {
        if (this != &other)
        {
            if (flags_ & OverwriteExisting)
            {
                std::error_code ec;
                std::filesystem::remove(path_, ec);
            }
            path_ = std::move(other.path_);
            flags_ = std::exchange(other.flags_, 0);
        }
        return *this;
    }

    std::string toString() const
    {
        return path_.string();
    }

    const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

    bool valid() const noexcept
    {
        return !path_.empty();
    }

private:
    ServerSocketPath() = default;

    std::filesystem::path path_;
    uint8_t flags_ = 0;
};

} // namespace casket