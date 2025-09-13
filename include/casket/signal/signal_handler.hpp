#pragma once

#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <functional>
#include <unordered_map>
#include <system_error>
#include <casket/nonstd/span.hpp>

namespace casket
{

class SignalHandler final
{
public:
    using SignalInfo = siginfo_t;
    using SignalCallback = std::function<void(int signum)>;

    SignalHandler()
        : signalFd_(-1)
    {
    }

    ~SignalHandler() noexcept
    {
        stop();
    }

    void registerSignal(int signum, SignalCallback callback, std::error_code& ec)
    {
        registerSignalImpl(signum, std::move(callback), ec);
        if (!ec) updateSignalDescriptor(ec);
    }

    void registerSignals(nonstd::span<int> signals, SignalCallback callback, std::error_code& ec)
    {
        for (int signum : signals)
        {
            registerSignalImpl(signum, callback, ec);
            if (ec) return;
        }
        updateSignalDescriptor(ec);
    }

    void unregisterSignal(int signum, std::error_code& ec)
    {
        callbacks_.erase(signum);
        updateSignalDescriptor(ec);
    }

    void unregisterSignals(nonstd::span<int> signals, std::error_code& ec)
    {
        for (int signum : signals)
        {
            callbacks_.erase(signum);
        }
        updateSignalDescriptor(ec);
    }

    void registerSignal(int signum, SignalCallback callback)
    {
        std::error_code ec;
        registerSignal(signum, std::move(callback), ec);
        if (ec)
        {
            throw std::system_error(ec);
        }
    }

    void registerSignals(nonstd::span<int> signals, SignalCallback callback)
    {
        std::error_code ec;
        registerSignals(signals, callback, ec);
        if (ec)
        {
            throw std::system_error(ec);
        }
    }

    void unregisterSignal(int signum)
    {
        std::error_code ec;
        unregisterSignal(signum, ec);
        if (ec)
        {
            throw std::system_error(ec);
        }
    }

    void unregisterSignals(nonstd::span<int> signals)
    {
        std::error_code ec;
        unregisterSignals(signals, ec);
        if (ec)
        {
            throw std::system_error(ec);
        }
    }

    int getDescriptor() const
    {
        return signalFd_;
    }

    void processSignals(std::error_code& ec) noexcept
    {
        ec.clear();

        if (signalFd_ == -1)
        {
            return;
        }

        signalfd_siginfo fdsi;

        while (true)
        {
            ssize_t s = read(signalFd_, &fdsi, sizeof(fdsi));

            if (s == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                else
                {
                    ec = std::error_code(errno, std::system_category());
                    break;
                }
            }
            else if (s != sizeof(fdsi))
            {
                ec = std::make_error_code(std::errc::io_error);
                break;
            }
            else
            {
                auto it = callbacks_.find(fdsi.ssi_signo);
                if (it != callbacks_.end())
                {
                    try
                    {
                        it->second(static_cast<int>(fdsi.ssi_signo));
                    }
                    catch (const std::exception& e)
                    {
                        ec = std::make_error_code(std::errc::operation_canceled);
                        // continue handling
                    }
                }
            }
        }
    }

    void processSignals()
    {
        std::error_code ec;
        processSignals(ec);

        if (ec)
        {
            throw std::system_error(ec);
        }
    }

    void stop() noexcept
    {
        closeSocket();

        sigset_t mask;
        sigfillset(&mask);
        sigprocmask(SIG_UNBLOCK, &mask, nullptr);

        callbacks_.clear();
    }

private:
    void registerSignalImpl(int signum, SignalCallback callback, std::error_code& ec) noexcept
    {
        callbacks_[signum] = std::move(callback);

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, signum);

        if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1)
        {
            ec = std::error_code(errno, std::system_category());
        }
    }

    void updateSignalDescriptor(std::error_code& ec) noexcept
    {
        sigset_t mask;
        sigemptyset(&mask);

        for (const auto& [signum, _] : callbacks_)
        {
            sigaddset(&mask, signum);
        }

        closeSocket();

        if (!callbacks_.empty())
        {
            signalFd_ = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
            if (signalFd_ == -1)
            {
                ec = std::error_code(errno, std::system_category());
            }
        }
    }

    void closeSocket() noexcept
    {
        if (signalFd_ != -1)
        {
            ::close(signalFd_);
            signalFd_ = -1;
        }
    }

private:
    int signalFd_;
    std::unordered_map<int, SignalCallback> callbacks_;
};

} // namespace casket