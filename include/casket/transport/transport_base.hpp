#pragma once
#include <cstddef>
#include <cerrno>
#include <cstdint>
#include <system_error>

namespace casket
{

template <typename Derived>
class TransportBase
{
public:
    ssize_t send(const uint8_t* data, size_t length)
    {
        return derived().sendImpl(data, length);
    }

    ssize_t recv(uint8_t* buffer, size_t length)
    {
        return derived().recvImpl(buffer, length);
    }

    bool isConnected() const
    {
        return derived().isConnectedImpl();
    }

    const std::error_code& lastError() const
    {
        return derived().lastErrorImpl();
    }

    int getFd() const
    {
        return derived().getFdImpl();
    }

    void close() noexcept
    {
        derived().closeImpl();
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