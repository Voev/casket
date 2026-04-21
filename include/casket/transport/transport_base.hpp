#pragma once
#include <cstddef>
#include <cerrno>
#include <cstdint>
#include <system_error>
#include <chrono>

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace casket
{

template <typename Derived>
class TransportBase
{
public:
    ssize_t send(const uint8_t* data, size_t length, std::error_code& ec) noexcept
    {
        return derived().sendImpl(data, length, ec);
    }

    ssize_t recv(uint8_t* buffer, size_t length, std::error_code& ec) noexcept
    {
        return derived().recvImpl(buffer, length, ec);
    }

    ssize_t sendmsg(const struct msghdr* msg, int flags, std::error_code& ec) noexcept
    {
        return derived().sendmsgImpl(msg, flags, ec);
    }

    ssize_t recvmsg(struct msghdr* msg, int flags, std::error_code& ec) noexcept
    {
        return derived().recvmsgImpl(msg, flags, ec);
    }

    bool isValid() const
    {
        return derived().isValidImpl();
    }

    int getFd() const
    {
        return derived().getFdImpl();
    }

    void close() noexcept
    {
        derived().closeImpl();
    }

    bool isConnected(std::chrono::milliseconds timeout, std::error_code& ec) noexcept
    {
        return derived().isConnectedImpl(timeout, ec);
    }

private:
    Derived& derived()
    {
        return static_cast<Derived&>(*this);
    }

    const Derived& derived() const
    {
        return static_cast<const Derived&>(*this);
    }
};

} // namespace casket