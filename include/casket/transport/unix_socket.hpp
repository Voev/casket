#pragma once
#include <string>
#include <utility>
#include <system_error>

#include <casket/transport/socket_ops.hpp>
#include <casket/transport/transport_base.hpp>
#include <casket/transport/server_socket_path.hpp>

#include <casket/utils/error_code.hpp>
#include <casket/nonstd/optional.hpp>

namespace casket
{

class UnixSocket final : public TransportBase<UnixSocket>
{
public:
    friend class TransportBase<UnixSocket>;

    UnixSocket() = default;

    ~UnixSocket() noexcept
    {
        closeImpl();
    }

    UnixSocket(const UnixSocket&) = delete;

    UnixSocket& operator=(const UnixSocket&) = delete;

    UnixSocket(UnixSocket&& other) noexcept
        : fd_(std::exchange(other.fd_, g_InvalidSocket))
    {
    }

    UnixSocket& operator=(UnixSocket&& other) noexcept
    {
        if (this != &other)
        {
            closeImpl();
            fd_ = std::exchange(other.fd_, g_InvalidSocket);
        }
        return *this;
    }

    bool connect(const std::string& path, const bool nonblock, std::error_code& ec)
    {
        if (path.empty())
        {
            SetSystemError(ec, std::errc::invalid_argument);
            return false;
        }

        fd_ = CreateSocket(AF_UNIX, SOCK_STREAM, 0, ec);
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

        SocketAddrUnixType addr{};
        if (!createAddress(addr, path.c_str(), ec))
        {
            closeImpl();
            return false;
        }

        if (Connect(fd_, (SocketAddrType*)&addr, sizeof(addr), ec) < 0)
        {
            closeImpl();
            return false;
        }

        return true;
    }

    bool listen(std::string path, int backlog, std::error_code& ec,
                ServerSocketPath::Flags flags = ServerSocketPath::Default)
    {
        if (path.empty())
        {
            SetSystemError(ec, std::errc::invalid_argument);
            return false;
        }

        auto socketPath = ServerSocketPath::create(std::move(path), flags, ec);
        if (ec)
        {
            return false;
        }

        fd_ = CreateSocket(AF_UNIX, SOCK_STREAM, 0, ec);
        if (fd_ < 0)
        {
            return false;
        }

        SetNonBlocking(fd_, true, ec);
        if (ec)
        {
            closeImpl();
            return false;
        }

        SocketAddrUnixType addr{};
        if (!createAddress(addr, socketPath.toString().c_str(), ec))
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

        socketPath_ = std::move(socketPath);
        ec.clear();
        return true;
    }

    UnixSocket accept(std::error_code& ec)
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::not_connected);
            return UnixSocket();
        }

        SocketType clientFd = Accept(fd_, nullptr, nullptr, ec);
        if (clientFd < 0)
        {
            return UnixSocket();
        }

        SetNonBlocking(clientFd, true, ec);
        if (ec)
        {
            return UnixSocket();
        }

        UnixSocket client;
        client.fd_ = clientFd;

        return client;
    }

private:
    ssize_t sendImpl(const uint8_t* data, size_t length, std::error_code& ec)
    {
        if (!isValidImpl())
        {
            SetSystemError(ec, std::errc::bad_file_descriptor);
            return -1;
        }

        ssize_t n = ::send(fd_, data, length, MSG_NOSIGNAL);
        if (n < 0)
        {
            ec = GetLastSystemError();
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
            SetSystemError(ec, std::errc::bad_file_descriptor);
            return -1;
        }

        ssize_t n = ::recv(fd_, buffer, len, MSG_DONTWAIT);
        if (n < 0)
        {
            ec = GetLastSystemError();
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
            SetSystemError(ec, std::errc::bad_file_descriptor);
            return -1;
        }

        ssize_t n = ::sendmsg(fd_, msg, flags | MSG_NOSIGNAL);
        if (n < 0)
        {
            ec = GetLastSystemError();
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
            SetSystemError(ec, std::errc::bad_file_descriptor);
            return -1;
        }

        ssize_t n = ::recvmsg(fd_, msg, flags | MSG_DONTWAIT);
        if (n < 0)
        {
            ec = GetLastSystemError();
        }
        else
        {
            ec.clear();
        }

        return n;
    }

    bool isValidImpl() const
    {
        return (fd_ != -1);
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
        }
    }

    static inline bool createAddress(SocketAddrUnixType& addr, const char* path, std::error_code& ec)
    {
        if (strlen(path) >= sizeof(addr.sun_path))
        {
            SetSystemError(ec, std::errc::filename_too_long);
            return false;
        }
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, path);
        return true;
    }

private:
    SocketType fd_{g_InvalidSocket};
    nonstd::optional<ServerSocketPath> socketPath_;
};

} // namespace casket