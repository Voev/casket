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
    SocketType sock;

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
    assert(sock != g_InvalidSocket);

    SocketType ret = ::connect(sock, addr, addrlen);
    if (ret == g_InvalidSocket)
    {
        ec = GetLastSystemError();
    }

    return ret;
}

inline bool Bind(SocketType sock, const SocketAddrType* addr, SocketLengthType addrlen, std::error_code& ec)
{
    assert(sock != g_InvalidSocket);

    if (0 > ::bind(sock, addr, addrlen))
    {
        ec = GetLastSystemError();
        return false;
    }

    return true;
}

inline bool Listen(SocketType sock, int backlog, std::error_code& ec)
{
    assert(sock != g_InvalidSocket);

    if (::listen(sock, backlog) < 0)
    {
        ec = GetLastSystemError();
        return false;
    }

    return true;
}

inline SocketType Accept(SocketType sock, SocketAddrType* addr, SocketLengthType* addrlen, std::error_code& ec)
{
    assert(sock != g_InvalidSocket);

    SocketType ret = ::accept(sock, addr, addrlen);
    if (ret == g_InvalidSocket)
    {
        ec = GetLastSystemError();
    }

    return ret;
}

inline void SetNonBlocking(SocketType sock, bool value, std::error_code& ec)
{
    assert(sock != g_InvalidSocket);

    int ret = ::fcntl(sock, F_GETFL, 0);
    if (ret < 0)
    {
        ec = GetLastSystemError();
    }
    else
    {
        int flag = (value ? (ret | O_NONBLOCK) : (ret & ~O_NONBLOCK));
        ret = ::fcntl(sock, F_SETFL, flag);
        if (ret < 0)
        {
            ec = GetLastSystemError();
        }
    }
}

inline int SetSocketOption(SocketType s, int level, int optname, void* optval, size_t optlen, std::error_code& ec)
{
    int ret = ::setsockopt(s, level, optname, optval, static_cast<SocketLengthType>(optlen));
    if (ret == -1)
    {
        ec = GetLastSystemError();
    }
    return ret;
}

inline int GetSocketOption(SocketType sock, int level, int optname, void* optval, size_t* optlen, std::error_code& ec)
{
    assert(sock != g_InvalidSocket);
    assert(optlen != nullptr);
    assert(optval != nullptr);

    SocketLengthType tmpOptlen = static_cast<SocketLengthType>(*optlen);
    int ret = ::getsockopt(sock, level, optname, optval, &tmpOptlen);

    if (ret == -1)
    {
        ec = GetLastSystemError();
    }

    *optlen = static_cast<std::size_t>(tmpOptlen);
    return ret;
}

inline std::error_code GetSocketError(SocketType sock)
{
    assert(sock != g_InvalidSocket);

    std::error_code ec;
    int socketError = 0;
    size_t optlen = sizeof(socketError);

    int ret = GetSocketOption(sock, SOL_SOCKET, SO_ERROR, &socketError, &optlen, ec);
    if (ret == -1)
    {
        return ec;
    }
    return std::error_code{socketError, std::system_category()};
}

} // namespace casket
