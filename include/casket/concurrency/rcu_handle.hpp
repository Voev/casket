#pragma once
#include <casket/concurrency/rcu.hpp>

namespace casket
{

template <typename T>
class RCUReadHandle final
{
public:
    RCUReadHandle(T* data, RCU& rcu, uint32_t threadId)
        : data_(data)
        , rcu_(rcu)
        , threadId_(threadId)
    {
    }

    ~RCUReadHandle() = default;

    RCUReadHandle(const RCUReadHandle&) = delete;
    RCUReadHandle& operator=(const RCUReadHandle&) = delete;

    RCUReadHandle(RCUReadHandle&& other) noexcept
        : data_(std::exchange(other.data_, nullptr))
        , rcu_(other.rcu_)
        , threadId_(other.threadId_)
    {
    }

    RCUReadHandle& operator=(RCUReadHandle&& other) noexcept
    {
        if (this != &other)
        {
            data_ = std::exchange(other.data_, nullptr);
            rcu_ = other.rcu_;
            threadId_ = other.threadId_;
        }
        return *this;
    }

    T* get() const noexcept
    {
        return data_;
    }
    T* operator->() const noexcept
    {
        return data_;
    }
    T& operator*() const noexcept
    {
        return *data_;
    }
    explicit operator bool() const noexcept
    {
        return data_ != nullptr;
    }

    void quiescent() noexcept
    {
        rcu_.quiescent(threadId_);
    }

private:
    T* data_;
    RCU& rcu_;
    uint32_t threadId_;
};

} // namespace casket