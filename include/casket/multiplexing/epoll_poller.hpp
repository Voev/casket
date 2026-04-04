#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <casket/multiplexing/connection_context.hpp>

namespace casket
{

template <typename ContextManager = DirectContext>
class EpollPollerTemplate : public PollerBase<EpollPollerTemplate<ContextManager>>
{
private:
    using Context = typename ContextManager::Context;
    using ContextPtr = typename ContextManager::ContextPtr;

    static constexpr size_t MAX_CONNECTIONS = 10000;
    static constexpr int MAX_EVENTS = 64;

    int epollFd_;
    std::array<ContextPtr, MAX_CONNECTIONS> fdToContext_;
    std::vector<epoll_event> epollEvents_;
    ContextManager contextManager_;

public:
    EpollPollerTemplate()
        : epollFd_(epoll_create1(0))
        , epollEvents_(MAX_EVENTS)
    {
        fdToContext_.fill(nullptr);

        if (epollFd_ < 0)
        {
            throw std::system_error(std::error_code(errno, std::system_category()), "Failed to create epoll");
        }
    }

    ~EpollPollerTemplate()
    {
        destroyImpl();
    }

    EpollPollerTemplate(const EpollPollerTemplate&) = delete;
    EpollPollerTemplate& operator=(const EpollPollerTemplate&) = delete;

    EpollPollerTemplate(EpollPollerTemplate&& other) noexcept
        : epollFd_(other.epollFd_)
        , fdToContext_(std::move(other.fdToContext_))
        , epollEvents_(std::move(other.epollEvents_))
        , contextManager_(std::move(other.contextManager_))
    {
        other.epollFd_ = -1;
        other.fdToContext_.fill(nullptr);
    }

    EpollPollerTemplate& operator=(EpollPollerTemplate&& other) noexcept
    {
        if (this != &other)
        {
            destroyImpl();
            epollFd_ = other.epollFd_;
            fdToContext_ = std::move(other.fdToContext_);
            epollEvents_ = std::move(other.epollEvents_);
            contextManager_ = std::move(other.contextManager_);
            other.epollFd_ = -1;
            other.fdToContext_.fill(nullptr);
        }
        return *this;
    }

    size_t getUsedContextsCount() const
    {
        return contextManager_.usedCount();
    }

    void clearContexts()
    {
        contextManager_.clear();
        fdToContext_.fill(nullptr);
    }

    void addImpl(int fd, EventType events, std::error_code& ec) noexcept
    {
        if (fd < 0 || fd >= static_cast<int>(MAX_CONNECTIONS))
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return;
        }

        if (fdToContext_[fd] != nullptr)
        {
            ec = std::make_error_code(std::errc::file_exists);
            return;
        }

        ContextPtr ctx = contextManager_.create();
        if (!ctx)
        {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return;
        }

        ctx->fd = fd;
        ctx->registeredEvents = events;
        ctx->generation++;

        epoll_event ev;
        ev.data.ptr = ctx;
        ev.events = convertEventsToEpoll(events);

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) != 0)
        {
            contextManager_.destroy(ctx);
            ec = std::make_error_code(static_cast<std::errc>(errno));
            return;
        }

        fdToContext_[fd] = ctx;
        ec.clear();
    }

    void addPtrImpl(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        addImpl(fd, events, ec);
        if (!ec && fdToContext_[fd])
        {
            fdToContext_[fd]->userData = ptr;
        }
    }

    void modifyImpl(int fd, EventType events, std::error_code& ec) noexcept
    {
        if (fd < 0 || fd >= static_cast<int>(MAX_CONNECTIONS))
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return;
        }

        ContextPtr ctx = fdToContext_[fd];
        if (!ctx)
        {
            ec = std::make_error_code(std::errc::no_such_file_or_directory);
            return;
        }

        ctx->registeredEvents = events;
        ctx->generation++;

        epoll_event ev;
        ev.data.ptr = ctx;
        ev.events = convertEventsToEpoll(events);

        if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) != 0)
        {
            ec = std::make_error_code(static_cast<std::errc>(errno));
            return;
        }

        ec.clear();
    }

    void modifyPtrImpl(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        modifyImpl(fd, events, ec);
        if (!ec && fdToContext_[fd])
        {
            fdToContext_[fd]->userData = ptr;
        }
    }

    void removeImpl(int fd, std::error_code& ec) noexcept
    {
        if (fd < 0 || fd >= static_cast<int>(MAX_CONNECTIONS))
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return;
        }

        ContextPtr ctx = fdToContext_[fd];
        if (!ctx)
        {
            ec = std::make_error_code(std::errc::no_such_file_or_directory);
            return;
        }

        if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) != 0)
        {
            ec = std::make_error_code(static_cast<std::errc>(errno));
            return;
        }

        contextManager_.destroy(ctx);
        fdToContext_[fd] = nullptr;
        ec.clear();
    }

    int waitImpl(PollEvent* events, int maxCount, int timeoutMs, std::error_code& ec) noexcept
    {
        if (maxCount > MAX_EVENTS)
        {
            maxCount = MAX_EVENTS;
        }

        int ret = epoll_wait(epollFd_, epollEvents_.data(), maxCount, timeoutMs);

        if (ret < 0)
        {
            ec = std::make_error_code(static_cast<std::errc>(errno));
            return -1;
        }

        for (int i = 0; i < ret; ++i)
        {
            const auto& epollEv = epollEvents_[i];
            auto* ctx = static_cast<ContextPtr>(epollEv.data.ptr);

            if (!ctx || ctx->generation == 0)
            {
                continue;
            }

            events[i].fd = ctx->fd;
            events[i].userData = ctx->userData;
            events[i].events = ctx->registeredEvents;
            events[i].revents = convertEpollToEvents(epollEv.events);
        }

        ec.clear();
        return ret;
    }

    bool isValidImpl() const noexcept
    {
        return epollFd_ != -1;
    }

    void destroyImpl() noexcept
    {
        if (epollFd_ != -1)
        {
            for (size_t i = 0; i < MAX_CONNECTIONS; ++i)
            {
                if (fdToContext_[i])
                {
                    contextManager_.destroy(fdToContext_[i]);
                    fdToContext_[i] = nullptr;
                }
            }
            ::close(epollFd_);
            epollFd_ = -1;
        }
        contextManager_.clear();
    }

    static uint32_t convertEventsToEpoll(EventType events) noexcept
    {
        uint32_t result = 0;
        if ((events & EventType::Readable) != EventType::None)
            result |= EPOLLIN;
        if ((events & EventType::Writable) != EventType::None)
            result |= EPOLLOUT;
        if ((events & EventType::Error) != EventType::None)
            result |= EPOLLERR;
        if ((events & EventType::HangUp) != EventType::None)
            result |= EPOLLHUP | EPOLLRDHUP;
        if ((events & EventType::EdgeTriggered) != EventType::None)
            result |= EPOLLET;
        return result;
    }

    static EventType convertEpollToEvents(uint32_t revents) noexcept
    {
        EventType result = EventType::None;
        if (revents & EPOLLIN)
            result |= EventType::Readable;
        if (revents & EPOLLOUT)
            result |= EventType::Writable;
        if (revents & EPOLLERR)
            result |= EventType::Error;
        if (revents & (EPOLLHUP | EPOLLRDHUP))
            result |= EventType::HangUp;
        return result;
    }
};

using EpollPoller = EpollPollerTemplate<DirectContext>;

using EpollPollerWithPool = EpollPollerTemplate<PoolContext>;

using EpollPollerWithVector = EpollPollerTemplate<VectorContext>;

} // namespace casket