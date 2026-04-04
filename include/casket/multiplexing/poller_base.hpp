#pragma once

#include <vector>
#include <system_error>
#include <atomic>
#include <memory>
#include <chrono>

namespace casket
{

template <typename Derived>
class PollerBase;

enum class EventType : uint32_t
{
    None = 0,
    Readable = 1 << 0,
    Writable = 1 << 1,
    Error = 1 << 2,
    HangUp = 1 << 3,
    Invalid = 1 << 4,
    EdgeTriggered = 1 << 5 // epoll only
};

inline EventType operator|(EventType a, EventType b)
{
    return static_cast<EventType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EventType operator&(EventType a, EventType b)
{
    return static_cast<EventType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline EventType& operator|=(EventType& a, EventType b)
{
    a = a | b;
    return a;
}

struct PollEvent
{
    int fd{-1};
    void* userData{nullptr};
    EventType events{EventType::None};
    EventType revents{EventType::None};
};

template <typename Derived>
class PollerBase
{
public:
    PollerBase() = default;
    ~PollerBase() = default;

    PollerBase(const PollerBase&) = delete;
    PollerBase& operator=(const PollerBase&) = delete;

    PollerBase(PollerBase&&) = default;
    PollerBase& operator=(PollerBase&&) = default;

    void add(int fd, EventType events, std::error_code& ec) noexcept
    {
        derived()->addImpl(fd, events, ec);
    }

    void add(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        derived()->addPtrImpl(ptr, fd, events, ec);
    }

    void modify(int fd, EventType events, std::error_code& ec) noexcept
    {
        derived()->modifyImpl(fd, events, ec);
    }

    void modify(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        derived()->modifyPtrImpl(ptr, fd, events, ec);
    }

    void remove(int fd, std::error_code& ec) noexcept
    {
        derived()->removeImpl(fd, ec);
    }

    int wait(PollEvent* events, int maxCount, int timeoutMs, std::error_code& ec) noexcept
    {
        return derived()->waitImpl(events, maxCount, timeoutMs, ec);
    }

    bool isValid() const noexcept
    {
        return derived()->isValidImpl();
    }

    void destroy() noexcept
    {
        derived()->destroyImpl();
    }

    void add(int fd, EventType events)
    {
        std::error_code ec;
        add(fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void add(void* ptr, int fd, EventType events)
    {
        std::error_code ec;
        add(ptr, fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void modify(int fd, EventType events)
    {
        std::error_code ec;
        modify(fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void modify(void* ptr, int fd, EventType events)
    {
        std::error_code ec;
        modify(ptr, fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void remove(int fd)
    {
        std::error_code ec;
        remove(fd, ec);
        if (ec)
            throw std::system_error(ec);
    }

private:
    inline Derived* derived()
    {
        return static_cast<Derived*>(this);
    }

    inline const Derived* derived() const
    {
        return static_cast<const Derived*>(this);
    }
};

} // namespace casket