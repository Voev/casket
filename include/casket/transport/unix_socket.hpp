#pragma once
#include <string>
#include <system_error>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <casket/transport/transport_base.hpp>
#include <casket/utils/error_code.hpp>

namespace casket
{

class UnixSocket final : public TransportBase<UnixSocket>
{
public:
    bool connect(const std::string& path)
    {
        isServer_ = false;
        path_ = path;

        fd_ = createSocket();
        if (fd_ < 0)
        {
            ec_ = GetLastSystemError();
            return false;
        }

        struct sockaddr_un addr{};
        createAddress(addr, path.c_str());

        if (::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            ec_ = GetLastSystemError();
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        connected_ = true;
        ec_.clear();
        return true;
    }

    bool listen(const std::string& path)
    {
        isServer_ = true;
        path_ = path;

        fd_ = createSocket();
        if (fd_ < 0)
        {
            ec_ = GetLastSystemError();
            return false;
        }

        struct sockaddr_un addr{};
        createAddress(addr, path.c_str());

        if (::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            ec_ = GetLastSystemError();
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        if (::listen(fd_, 128) < 0)
        {
            ec_ = GetLastSystemError();
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        connected_ = true;
        ec_.clear();
        return true;
    }

    UnixSocket accept()
    {
        if (!isServer_ || fd_ < 0)
        {
            return UnixSocket();
        }

        int clientFd = ::accept(fd_, nullptr, nullptr);
        if (clientFd < 0)
        {
            ec_ = GetLastSystemError();
            return UnixSocket();
        }

        int flags = fcntl(clientFd, F_GETFL, 0);
        fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

        UnixSocket client;
        client.fd_ = clientFd;
        client.connected_ = true;
        client.ec_.clear();

        return client;
    }

    ssize_t sendImpl(const uint8_t* data, size_t length)
    {
        if (!connected_)
        {
            SetSystemError(ec_, std::errc::not_connected);
            return -1;
        }

        ssize_t n = ::send(fd_, data, length, MSG_NOSIGNAL);
        if (n < 0)
        {
            ec_ = GetLastSystemError();
            if (ec_ != std::errc::resource_unavailable_try_again)
            {
                connected_ = false;
            }
        }
        else
        {
            ec_.clear();
        }

        return n;
    }

    ssize_t recvImpl(uint8_t* buffer, size_t len)
    {
        if (!connected_)
        {
            SetSystemError(ec_, std::errc::not_connected);
            return -1;
        }

        ssize_t n = ::recv(fd_, buffer, len, MSG_DONTWAIT);
        if (n < 0)
        {
            ec_ = GetLastSystemError();
            if (ec_ != std::errc::resource_unavailable_try_again)
            {
                connected_ = false;
            }
        }
        else if (n == 0)
        {
            SetSystemError(ec_, std::errc::connection_reset);
            connected_ = false;
        }
        else
        {
            ec_.clear();
        }

        return n;
    }

    bool isConnectedImpl() const
    {
        return (connected_ && fd_ >= 0);
    }

    int getFdImpl() const
    {
        return fd_;
    }

    const std::error_code& lastErrorImpl() const
    {
        return ec_;
    }

    void closeImpl() noexcept
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
        connected_ = false;
        ec_.clear();

        if (isServer_ && !path_.empty())
        {
            ::unlink(path_.c_str());
        }
    }

    bool isValid() const
    {
        return fd_ != -1;
    }

private:
    inline int createSocket() noexcept
    {
        return ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    }

    static inline void createAddress(sockaddr_un& addr, const char* path)
    {
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
        addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    }

private:
    std::string path_;
    std::error_code ec_;
    int fd_{-1};
    bool connected_{false};
    bool isServer_{false};
};

} // namespace casket