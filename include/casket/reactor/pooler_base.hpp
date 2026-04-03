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
    int fd;
    void* user_data;
    EventType events;
    EventType revents;

    PollEvent()
        : fd(-1)
        , user_data(nullptr)
        , events(EventType::None)
        , revents(EventType::None)
    {
    }
};

template <typename Derived>
class PollerBase
{
public:
    PollerBase() = default;
    virtual ~PollerBase() = default;

    PollerBase(const PollerBase&) = delete;
    PollerBase& operator=(const PollerBase&) = delete;

    PollerBase(PollerBase&&) = default;
    PollerBase& operator=(PollerBase&&) = default;

    void add(int fd, EventType events, std::error_code& ec) noexcept
    {
        static_cast<Derived*>(this)->add_impl(fd, events, ec);
    }

    void add(int fd, EventType events)
    {
        std::error_code ec;
        add(fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void add(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        static_cast<Derived*>(this)->add_ptr_impl(ptr, fd, events, ec);
    }

    void add(void* ptr, int fd, EventType events)
    {
        std::error_code ec;
        add(ptr, fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void modify(int fd, EventType events, std::error_code& ec) noexcept
    {
        static_cast<Derived*>(this)->modify_impl(fd, events, ec);
    }

    void modify(int fd, EventType events)
    {
        std::error_code ec;
        modify(fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void modify(void* ptr, int fd, EventType events, std::error_code& ec) noexcept
    {
        static_cast<Derived*>(this)->modify_ptr_impl(ptr, fd, events, ec);
    }

    void modify(void* ptr, int fd, EventType events)
    {
        std::error_code ec;
        modify(ptr, fd, events, ec);
        if (ec)
            throw std::system_error(ec);
    }

    void remove(int fd, std::error_code& ec) noexcept
    {
        static_cast<Derived*>(this)->remove_impl(fd, ec);
    }

    void remove(int fd)
    {
        std::error_code ec;
        remove(fd, ec);
        if (ec)
            throw std::system_error(ec);
    }

    int wait(PollEvent* events, int maxCount, int timeoutMs, std::error_code& ec)
    {
        return static_cast<Derived*>(this)->wait_impl(events, maxCount, timeoutMs, ec);
    }

    bool is_valid() const noexcept
    {
        return static_cast<const Derived*>(this)->is_valid_impl();
    }

    void destroy() noexcept
    {
        static_cast<Derived*>(this)->destroy_impl();
    }

protected:
    void add_impl(int fd, EventType events, std::error_code& ec) noexcept;
    void add_ptr_impl(void* ptr, int fd, EventType events, std::error_code& ec) noexcept;
    void modify_impl(int fd, EventType events, std::error_code& ec) noexcept;
    void modify_ptr_impl(void* ptr, int fd, EventType events, std::error_code& ec) noexcept;
    void remove_impl(int fd, std::error_code& ec) noexcept;
    int wait_impl(PollEvent* events, int maxCount, int timeoutMs, std::error_code& ec);
    bool is_valid_impl() const noexcept;
    void destroy_impl() noexcept;
};

} // namespace casket