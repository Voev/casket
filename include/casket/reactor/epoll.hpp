#pragma once

#include <unistd.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <casket/reactor/pooler_base.hpp>
#include <casket/reactor/object_pool.hpp>

namespace casket
{

class EpollPoller : public PollerBase<EpollPoller>
{
private:
    struct ConnectionContext
    {
        int fd;
        void* userData;
        EventType registeredEvents;
        uint32_t generation;
        epoll_event cachedEpollEvent;

        ConnectionContext()
            : fd(-1)
            , userData(nullptr)
            , registeredEvents(EventType::None)
            , generation(0)
        {
            cachedEpollEvent.events = 0;
            cachedEpollEvent.data.ptr = nullptr;
        }
    };

    static constexpr size_t MAX_CONNECTIONS = 10000;
    static constexpr int MAX_EVENTS = 64;

    int epollFd_;
    ObjectPool<ConnectionContext, MAX_CONNECTIONS> contextPool_;
    std::array<ConnectionContext*, MAX_CONNECTIONS> fdToContext_;
    std::vector<epoll_event> epollEvents_;

public:
    EpollPoller()
        : epollFd_(epoll_create1(0))
        , epollEvents_(MAX_EVENTS)
    {
        if (epollFd_ < 0)
        {
            throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), "Failed to create epoll");
        }
        fdToContext_.fill(nullptr);
    }

    ~EpollPoller()
    {
        destroy_impl();
    }

    void add_impl(int fd, EventType events, std::error_code& ec) noexcept
    {
        if (fd < 0 || fd >= MAX_CONNECTIONS)
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return;
        }

        if (fdToContext_[fd] != nullptr)
        {
            ec = std::make_error_code(std::errc::file_exists);
            return;
        }

        ConnectionContext* ctx = contextPool_.allocate();
        if (!ctx)
        {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return;
        }

        ctx->fd = fd;
        ctx->registeredEvents = events;
        ctx->userData = nullptr;
        ctx->generation++;

        epoll_event ev;
        ev.data.ptr = ctx;
        ev.events = convert_events(events);
        ctx->cachedEpollEvent = ev;

        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) != 0)
        {
            contextPool_.deallocate(ctx);
            ec = std::make_error_code(static_cast<std::errc>(errno));
            return;
        }

        fdToContext_[fd] = ctx;
        ec.clear();
    }

    void add_ptr_impl(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        add_impl(fd, events, ec);
        if (!ec)
        {
            fdToContext_[fd]->userData = ptr;
        }
    }

    void modify_impl(int fd, EventType events, std::error_code& ec) noexcept
    {
        if (fd < 0 || fd >= MAX_CONNECTIONS)
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return;
        }

        ConnectionContext* ctx = fdToContext_[fd];
        if (!ctx)
        {
            ec = std::make_error_code(std::errc::no_such_file_or_directory);
            return;
        }

        ctx->registeredEvents = events;
        epoll_event ev = ctx->cachedEpollEvent;
        ev.events = convert_events(events);

        if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) != 0)
        {
            ec = std::make_error_code(static_cast<std::errc>(errno));
            return;
        }

        ec.clear();
    }

    void modify_ptr_impl(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        modify_impl(fd, events, ec);
        if (!ec)
        {
            fdToContext_[fd]->userData = ptr;
        }
    }

    void remove_impl(int fd, std::error_code& ec) noexcept
    {
        if (fd < 0 || fd >= MAX_CONNECTIONS)
        {
            ec = std::make_error_code(std::errc::invalid_argument);
            return;
        }

        ConnectionContext* ctx = fdToContext_[fd];
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

        fdToContext_[fd] = nullptr;
        contextPool_.deallocate(ctx);
        ec.clear();
    }

    int wait_impl(PollEvent* events, int maxCount, int timeoutMs, std::error_code& ec)
    {
        int ret = epoll_wait(epollFd_, epollEvents_.data(), std::min(maxCount, MAX_EVENTS), timeoutMs);

        if (ret < 0)
        {
            ec = std::make_error_code(static_cast<std::errc>(errno));
            return -1;
        }

        for (int i = 0; i < ret; ++i)
        {
            const auto& epoll_ev = epollEvents_[i];
            auto* ctx = static_cast<ConnectionContext*>(epoll_ev.data.ptr);

            if (!ctx || ctx->generation == 0)
                continue;

            events[i].fd = ctx->fd;
            events[i].user_data = ctx->userData;
            events[i].events = ctx->registeredEvents;
            events[i].revents = convert_revents(epoll_ev.events);
        }

        ec.clear();
        return ret;
    }

    bool is_valid_impl() const noexcept
    {
        return epollFd_ != -1;
    }

    void destroy_impl() noexcept
    {
        if (epollFd_ != -1)
        {
            ::close(epollFd_);
            epollFd_ = -1;
        }

        for (size_t i = 0; i < MAX_CONNECTIONS; ++i)
        {
            if (fdToContext_[i])
            {
                contextPool_.deallocate(fdToContext_[i]);
                fdToContext_[i] = nullptr;
            }
        }
    }

private:

    static uint32_t convert_events(EventType events)
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

    static EventType convert_revents(uint32_t revents)
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

} // namespace casket