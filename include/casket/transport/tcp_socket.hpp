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
#include <casket/transport/select.hpp>
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

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    TcpSocket(TcpSocket&& other) noexcept
        : fd_(std::exchange(other.fd_, g_InvalidSocket))
        , isNonBlocking_(std::exchange(other.isNonBlocking_, false))
    {
    }

    TcpSocket& operator=(TcpSocket&& other) noexcept
    {
        if (this != &other)
        {
            closeImpl();
            fd_ = std::exchange(other.fd_, g_InvalidSocket);
            isNonBlocking_ = std::exchange(other.isNonBlocking_, false);
        }
        return *this;
    }

    bool connect(const std::string& address, uint16_t port, bool nonblock, std::error_code& ec)
    {
        isNonBlocking_ = nonblock;

        if (address.empty())
        {
            SetSystemError(ec, std::errc::invalid_argument);
            return false;
        }

        fd_ = CreateSocket(AF_INET, SOCK_STREAM, 0, ec);
        if (fd_ < 0)
        {
            return false;
        }

        SetNonBlocking(fd_, nonblock, ec);
        if (ec)
        {
            closeImpl();
            return false;
        }

        SocketAddrIn4Type addr{};
        if (!createAddress(addr, address.c_str(), port, ec))
        {
            closeImpl();
            return false;
        }

        if (Connect(fd_, (SocketAddrType*)&addr, sizeof(addr), ec) < 0)
        {
            if (nonblock && ec == std::errc::operation_in_progress)
            {
                ec.clear();
                return true;
            }
            closeImpl();
            return false;
        }

        ec.clear();
        return true;
    }

    bool listen(const std::string& address, uint16_t port, int backlog, std::error_code& ec)
    {
        fd_ = CreateSocket(AF_INET, SOCK_STREAM, 0, ec);
        if (fd_ < 0)
        {
            return false;
        }

        int opt = 1;
        if (SetSocketOption(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt), ec) < 0)
        {
            closeImpl();
            return false;
        }

        isNonBlocking_ = true;
        SetNonBlocking(fd_, true, ec);
        if (ec)
        {
            closeImpl();
            return false;
        }

        SocketAddrIn4Type addr{};
        if (!createAddress(addr, address.c_str(), port, ec))
        {
            closeImpl();
            return false;
        }

        if (!Bind(fd_, (SocketAddrType*)&addr, sizeof(addr), ec))
        {
            closeImpl();
            return false;
        }

        if (!Listen(fd_, backlog, ec))
        {
            closeImpl();
            return false;
        }

        ec.clear();
        return true;
    }

    TcpSocket accept(std::error_code& ec)
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::not_connected);
            return TcpSocket();
        }

        SocketType clientFd = Accept(fd_, nullptr, nullptr, ec);
        if (clientFd < 0)
        {
            return TcpSocket();
        }

        SetNonBlocking(clientFd, true, ec);
        if (ec)
        {
            CloseSocket(clientFd);
            return TcpSocket();
        }

        TcpSocket client;
        client.fd_ = clientFd;
        client.isNonBlocking_ = true;
        return client;
    }

private:
    ssize_t sendImpl(const uint8_t* data, size_t length, std::error_code& ec) noexcept
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::not_connected);
            return -1;
        }

        ssize_t n = ::send(fd_, data, length, MSG_NOSIGNAL);
        if (n < 0)
        {
            ec = GetLastSystemError();
            if (isNonBlocking_ &&
                (ec == std::errc::resource_unavailable_try_again || ec == std::errc::operation_would_block))
            {
                return 0;
            }
        }
        else
        {
            ec.clear();
        }

        return n;
    }

    ssize_t recvImpl(uint8_t* buffer, size_t len, std::error_code& ec)
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::not_connected);
            return -1;
        }

        int flags = isNonBlocking_ ? MSG_DONTWAIT : 0;
        ssize_t n = ::recv(fd_, buffer, len, flags);

        if (n < 0)
        {
            ec = GetLastSystemError();
            if (isNonBlocking_ &&
                (ec == std::errc::resource_unavailable_try_again || ec == std::errc::operation_would_block))
            {
                return 0;
            }
        }
        else
        {
            ec.clear();
        }

        return n;
    }

    ssize_t sendmsgImpl(const struct msghdr* msg, int flags, std::error_code& ec)
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::not_connected);
            return -1;
        }

        ssize_t n = ::sendmsg(fd_, msg, flags | MSG_NOSIGNAL);
        if (n < 0)
        {
            ec = GetLastSystemError();
            if (isNonBlocking_ &&
                (ec == std::errc::resource_unavailable_try_again || ec == std::errc::operation_would_block))
            {
                return 0;
            }
        }
        else
        {
            ec.clear();
        }

        return n;
    }

    ssize_t recvmsgImpl(struct msghdr* msg, int flags, std::error_code& ec)
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::not_connected);
            return -1;
        }

        int actualFlags = flags;
        if (isNonBlocking_)
        {
            actualFlags |= MSG_DONTWAIT;
        }

        ssize_t n = ::recvmsg(fd_, msg, actualFlags);
        if (n < 0)
        {
            ec = GetLastSystemError();
            if (isNonBlocking_ &&
                (ec == std::errc::resource_unavailable_try_again || ec == std::errc::operation_would_block))
            {
                return 0;
            }
        }
        else
        {
            ec.clear();
        }

        return n;
    }

    bool isConnectedImpl(std::chrono::milliseconds timeout, std::error_code& ec) noexcept
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::not_connected);
            return false;
        }

        if (!isNonBlocking_)
        {
            ec.clear();
            return true;
        }

        WaitSocket(fd_, false, timeout, ec);
        if (ec)
        {
            return false;
        }

        ec = GetSocketError(fd_);
        return !ec;
    }

    bool isValidImpl() const
    {
        return (fd_ != g_InvalidSocket);
    }

    int getFdImpl() const
    {
        return fd_;
    }

    void closeImpl() noexcept
    {
        if (fd_ != g_InvalidSocket)
        {
            CloseSocket(fd_);
            fd_ = g_InvalidSocket;
            isNonBlocking_ = false;
        }
    }

    static inline bool createAddress(sockaddr_in& addr, const char* host, int port, std::error_code& ec)
    {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (strcmp(host, "0.0.0.0") == 0)
        {
            addr.sin_addr.s_addr = INADDR_ANY;
            return true;
        }

        int ret = inet_pton(AF_INET, host, &addr.sin_addr);
        if (ret == 0)
        {
            SetSystemError(ec, std::errc::invalid_argument);
            return false;
        }
        else if (ret < 0)
        {
            ec = GetLastSystemError();
            return false;
        }

        return true;
    }

private:
    int fd_{g_InvalidSocket};
    bool isNonBlocking_{false};
};

} // namespace casket