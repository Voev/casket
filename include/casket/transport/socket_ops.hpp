#pragma once

#include <assert.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <fcntl.h>

#include <system_error>
#include <casket/utils/error_code.hpp>

namespace casket
{

static constexpr int g_InvalidSocket = -1;

typedef int SocketType;
typedef sockaddr SocketAddrType;
typedef socklen_t SocketLengthType;
typedef sockaddr_in SocketAddrIn4Type;
typedef sockaddr_in6 SocketAddrIn6Type;
typedef sockaddr_un SocketAddrUnixType;

typedef struct addrinfo AddressInfo;
typedef struct hostent HostEntry;

inline SocketType CreateSocket(int domain, int socktype, int protocol, std::error_code& ec)
{
    SocketType sock = g_InvalidSocket;

    sock = ::socket(domain, socktype, protocol);
    if (sock == g_InvalidSocket)
    {
        ec = GetLastSystemError();
    }

    return sock;
}

inline void CloseSocket(SocketType sock)
{
    assert(sock != g_InvalidSocket);
    ::close(sock);
}

inline SocketType Connect(SocketType sock, const SocketAddrType* addr, SocketLengthType addrlen, std::error_code& ec)
{
    if (sock == g_InvalidSocket)
    {
        ec = std::make_error_code(std::errc::invalid_argument);
        return g_InvalidSocket;
    }

    SocketType ret = ::connect(sock, addr, addrlen);
    if (ret == g_InvalidSocket)
    {
        ec = GetLastSystemError();
    }

    return ret;
}

inline SocketType Accept(SocketType sock, SocketAddrType* addr, SocketLengthType* addrlen, std::error_code& ec)
{
    if (sock == g_InvalidSocket)
    {
        ec = std::make_error_code(std::errc::invalid_argument);
        return g_InvalidSocket;
    }

    SocketType ret = ::accept(sock, addr, addrlen);
    if (ret == g_InvalidSocket)
    {
        ec = GetLastSystemError();
    }

    return ret;
}

inline void SetNonBlocking(SocketType s, bool value, std::error_code& ec)
{
    if (s == g_InvalidSocket)
    {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    int ret = ::fcntl(s, F_GETFL, 0);
    if (ret < 0)
    {
        ec = GetLastSystemError();
    }
    else
    {
        int flag = (value ? (ret | O_NONBLOCK) : (ret & ~O_NONBLOCK));
        ret = ::fcntl(s, F_SETFL, flag);
        if (ret < 0)
        {
            ec = GetLastSystemError();
        }
    }
}

inline int GetSocketOption(SocketType s, int level, int optname, void* optval, size_t* optlen, std::error_code& ec)
{
    assert(optlen != nullptr);
    assert(optval != nullptr);

    SocketLengthType tmpOptlen = static_cast<SocketLengthType>(*optlen);
    int ret = ::getsockopt(s, level, optname, optval, &tmpOptlen);

    if (ret == -1)
    {
        ec = GetLastSystemError();
    }
    else
    {
        ec.clear();
    }

    *optlen = static_cast<std::size_t>(tmpOptlen);
    return ret;
}

inline std::error_code GetSocketError(SocketType s)
{
    std::error_code ec;
    int socketError = 0;
    size_t optlen = sizeof(socketError);

    int ret = GetSocketOption(s, SOL_SOCKET, SO_ERROR, &socketError, &optlen, ec);
    if (ret == -1)
    {
        return ec;
    }
    return std::error_code{socketError, std::system_category()};
}

} // namespace casket
