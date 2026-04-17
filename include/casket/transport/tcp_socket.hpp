#pragma once

#include <string>
#include <system_error>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <casket/transport/transport_base.hpp>
#include <casket/utils/error_code.hpp>

namespace casket
{

class TcpSocket final : public TransportBase<TcpSocket>
{
public:
    friend class TransportBase<TcpSocket>;

    TcpSocket() = default;

    ~TcpSocket() noexcept
    {
        closeImpl();
    }

    explicit TcpSocket(int fd)
        : fd_(fd)
        , connected_(fd >= 0)
    {
        if (connected_)
        {
            setNonBlocking(fd_);
        }
    }

    TcpSocket(const TcpSocket&) = delete;

    TcpSocket& operator=(const TcpSocket&) = delete;

    TcpSocket(TcpSocket&& other) noexcept
        : ec_(std::move(other.ec_))
        , fd_(std::exchange(other.fd_, -1))
        , connected_(std::exchange(other.connected_, false))
    {
    }

    TcpSocket& operator=(TcpSocket&& other) noexcept
    {
        if (this != &other)
        {
            closeImpl();

            ec_ = std::move(other.ec_);
            fd_ = std::exchange(other.fd_, -1);
            connected_ = std::exchange(other.connected_, false);
        }
        return *this;
    }

    bool connect(const std::string& host, int port)
    {
        fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd_ < 0)
        {
            ec_ = GetLastSystemError();
            return false;
        }

        struct sockaddr_in addr{};
        createAddress(addr, host.c_str(), port);

        if (::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            if (errno != EINPROGRESS)
            {
                ec_ = GetLastSystemError();
                ::close(fd_);
                fd_ = -1;
                return false;
            }
        }

        connected_ = true;
        ec_.clear();
        return true;
    }

    bool listen(int port, const std::string& address = "0.0.0.0")
    {
        fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd_ < 0)
        {
            ec_ = GetLastSystemError();
            return false;
        }

        int opt = 1;
        if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            ec_ = GetLastSystemError();
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        struct sockaddr_in addr{};
        createAddress(addr, address.c_str(), port);

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

    TcpSocket accept()
    {
        if (fd_ < 0)
        {
            return TcpSocket();
        }

        int clientFd = ::accept(fd_, nullptr, nullptr);
        if (clientFd < 0)
        {
            ec_ = GetLastSystemError();
            return TcpSocket();
        }

        setNonBlocking(clientFd);

        TcpSocket client;
        client.fd_ = clientFd;
        client.connected_ = true;
        client.ec_.clear();

        return client;
    }

private:
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

    ssize_t sendmsgImpl(const struct msghdr* msg, int flags = 0)
    {
        if (!connected_)
        {
            SetSystemError(ec_, std::errc::not_connected);
            return -1;
        }

        ssize_t n = ::sendmsg(fd_, msg, flags | MSG_NOSIGNAL);
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

    ssize_t recvmsgImpl(struct msghdr* msg, int flags = 0)
    {
        if (!connected_)
        {
            SetSystemError(ec_, std::errc::not_connected);
            return -1;
        }

        ssize_t n = ::recvmsg(fd_, msg, flags | MSG_DONTWAIT);
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
    }

    static inline void createAddress(sockaddr_in& addr, const char* host, int port)
    {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        if (strcmp(host, "0.0.0.0") == 0)
        {
            addr.sin_addr.s_addr = INADDR_ANY;
        }
        else
        {
            inet_pton(AF_INET, host, &addr.sin_addr);
        }
    }

    static inline void setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

private:
    std::error_code ec_;
    int fd_{-1};
    bool connected_{false};
};

} // namespace casket