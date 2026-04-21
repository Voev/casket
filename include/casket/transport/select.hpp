#pragma once
#include <chrono>
#include <casket/utils/error_code.hpp>
#include <casket/transport/socket_ops.hpp>

namespace casket
{

void WaitSocket(SocketType socket, bool read, std::chrono::milliseconds timeout, std::error_code& ec)
{
    if (socket == g_InvalidSocket)
    {
        SetSystemError(ec, std::errc::invalid_argument);
        return;
    }

    fd_set confds;
    struct timeval tv;

    FD_ZERO(&confds);
    FD_SET(socket, &confds);

    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);

    int ret;
    do
    {
        ret = select(socket + 1, read ? &confds : nullptr, read ? nullptr : &confds, nullptr, &tv);
    } while (ret == -1 && errno == EINTR);

    if (ret == 0)
    {
        SetSystemError(ec, std::errc::timed_out);
    }
    else if (ret == -1)
    {
        ec = GetLastSystemError();
    }
    else
    {
        if (FD_ISSET(socket, &confds))
        {
            ec.clear();
        }
        else
        {
            SetSystemError(ec, std::errc::timed_out);
        }
    }
}

} // namespace casket