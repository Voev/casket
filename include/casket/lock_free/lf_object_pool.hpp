#pragma once
#include <algorithm>
#include <atomic>
#include <vector>
#include <cassert>
#include <stdexcept>
#include <new>

namespace casket::lf
{

template <typename T>
class ObjectPool
{
public:
    struct CountedNodePtr
    {
        uint32_t externalCnt = 1;
        int32_t nodeIdx = -1;

        CountedNodePtr() noexcept = default;
        explicit CountedNodePtr(int32_t idx) noexcept
            : nodeIdx(idx)
        {
        }

        bool operator==(const CountedNodePtr& other) const
        {
            return externalCnt == other.externalCnt && nodeIdx == other.nodeIdx;
        }

        bool operator!=(const CountedNodePtr& other) const
        {
            return !(*this == other);
        }
    };

    struct Node
    {
        T obj;
        int nodeIdx;
        std::atomic<CountedNodePtr> next;

        template <typename... Args>
        explicit Node(Args&&... args)
            : obj(std::forward<Args>(args)...)
            , nodeIdx(-1)
            , next(CountedNodePtr())
        {
        }

        Node(Node&& other) noexcept
            : obj(std::move(other.obj))
            , nodeIdx(other.nodeIdx)
            , next(other.next.load())
        {
            other.nodeIdx = -1;
        }

        Node& operator=(Node&& other) noexcept
        {
            if (this != &other)
            {
                obj = std::move(other.obj);
                nodeIdx = other.nodeIdx;
                next.store(other.next.load());
                other.nodeIdx = -1;
            }
            return *this;
        }

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
    };

    template <typename... Args>
    ObjectPool(size_t num, Args&&... args)
    {
        if (num == 0)
        {
            throw std::invalid_argument("Pool size must be greater than 0");
        }

        nodes_.reserve(num);

        for (size_t i = 0; i < num; ++i)
        {
            nodes_.emplace_back(std::forward<Args>(args)...);
            nodes_.back().nodeIdx = static_cast<int>(i);

            if (i > 0)
            {
                CountedNodePtr prev(static_cast<int>(i - 1));
                nodes_[i].next.store(prev, std::memory_order_relaxed);
            }
        }

        CountedNodePtr head(static_cast<int>(nodes_.size() - 1));
        entry_.store(head, std::memory_order_release);
        activeCount_.store(0, std::memory_order_relaxed);
    }

    ~ObjectPool() noexcept
    {
        assert(activeCount_.load(std::memory_order_relaxed) == 0 && "Objects not returned to pool!");
    }

    T* acquire() noexcept
    {
        CountedNodePtr ptr = getCountedNode();
        T* obj = getObject(ptr);

        if (obj)
        {
            activeCount_.fetch_add(1, std::memory_order_relaxed);
        }

        return obj;
    }

    void release(T* obj) noexcept
    {
        if (!obj)
            return;

        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            if (&nodes_[i].obj == obj)
            {
                CountedNodePtr ptr(static_cast<int>(i));
                reclaim(ptr);
                activeCount_.fetch_sub(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    size_t size() const noexcept
    {
        return nodes_.size();
    }

    size_t activeCount() const noexcept
    {
        return activeCount_.load(std::memory_order_relaxed);
    }

    size_t freeCount() const noexcept
    {
        return size() - activeCount();
    }

    bool empty() const noexcept
    {
        return activeCount() == 0;
    }

private:
    void reclaim(CountedNodePtr nd) noexcept
    {
        if (nd.nodeIdx < 0)
            return;

        CountedNodePtr oldHead = entry_.load(std::memory_order_acquire);
        CountedNodePtr newHead = nd;
        newHead.externalCnt = 1;

        do
        {
            nodes_[nd.nodeIdx].next.store(oldHead, std::memory_order_relaxed);
        } while (!entry_.compare_exchange_weak(oldHead, newHead, std::memory_order_release, std::memory_order_acquire));
    }

    CountedNodePtr getCountedNode() noexcept
    {
        if (nodes_.empty())
            return CountedNodePtr();

        const int MAX_RETRIES = 10000;
        int retries = 0;

        while (retries++ < MAX_RETRIES)
        {
            CountedNodePtr old = entry_.load(std::memory_order_acquire);

            if (old.nodeIdx < 0)
            {
                return CountedNodePtr();
            }

            CountedNodePtr increased = old;
            increased.externalCnt++;

            if (!entry_.compare_exchange_weak(old, increased, std::memory_order_acquire, std::memory_order_relaxed))
            {
                continue;
            }

            Node* node = &nodes_[increased.nodeIdx];
            CountedNodePtr next = node->next.load(std::memory_order_acquire);

            if (entry_.compare_exchange_strong(increased, next, std::memory_order_release, std::memory_order_relaxed))
            {
                return increased;
            }
        }

        return CountedNodePtr();
    }

    T* getObject(const CountedNodePtr& nodeCnt) noexcept
    {
        if (nodeCnt.nodeIdx < 0)
            return nullptr;
        return &nodes_[nodeCnt.nodeIdx].obj;
    }

private:
    std::vector<Node> nodes_;
    alignas(64) std::atomic<CountedNodePtr> entry_{CountedNodePtr()};
    alignas(64) std::atomic<size_t> activeCount_{0};
};

} // namespace casket::lf