#pragma once
#include <casket/multiplexing/poller_base.hpp>
#include <casket/multiplexing/memory_policy.hpp>

namespace casket
{

// Forward declaration
template <typename MemoryPolicy>
class ConnectionContext;

struct ContextData
{
    int fd{-1};
    void* userData{nullptr};
    EventType registeredEvents{EventType::None};
    uint32_t generation{0};
};

template <typename MemoryPolicy>
class ConnectionContext
{
private:
    MemoryPolicy policy_;

public:
    using Context = ContextData;
    using ContextPtr = Context*;

    ContextPtr create()
    {
        return policy_.create();
    }

    void destroy(ContextPtr ptr)
    {
        policy_.destroy(ptr);
    }

    void clear()
    {
        policy_.clear();
    }

    MemoryPolicy& getPolicy()
    {
        return policy_;
    }

    const MemoryPolicy& getPolicy() const
    {
        return policy_;
    }

    size_t usedCount() const
    {
        return policy_.usedCount();
    }
};

using DirectContext = ConnectionContext<DirectMemoryPolicy<ContextData>>;

using PoolContext = ConnectionContext<PoolMemoryPolicy<ContextData, 10000>>;

using VectorContext = ConnectionContext<VectorMemoryPolicy<ContextData>>;

} // namespace casket