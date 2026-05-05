#pragma once
#include <algorithm>
#include <atomic>
#include <vector>
#include <memory>

namespace casket::lf
{

template <typename T>
class ObjectPool
{
public:
    struct Node;

    struct CountedNodePtr
    {
        uint32_t externalCnt = 1;
        int32_t nodeIdx = -1;

        CountedNodePtr() noexcept = default;
        explicit CountedNodePtr(int32_t idx) noexcept
            : nodeIdx(idx)
        {
        }
    };

    struct Node
    {
        T* obj;
        int nodeIdx;
        CountedNodePtr next;

        explicit Node(T* s)
            : obj(s)
            , nodeIdx(-1)
        {
        }
    };

    template <typename... Args>
    ObjectPool(size_t num, Args&&... args)
    {
        if (!num)
        {
            throw std::invalid_argument("passed invalid argument");
        }

        objs_.reserve(num);
        nodes_.reserve(num);

        for (size_t i = 0; i < num; ++i)
        {
            objs_.emplace_back(new T(std::forward<Args>(args)...));
            nodes_.emplace_back(objs_.back().get());
            nodes_.back().nodeIdx = static_cast<int>(i);

            if (i > 0)
            {
                nodes_[i].next.nodeIdx = static_cast<int>(i - 1);
            }
        }

        CountedNodePtr head(static_cast<int>(nodes_.size() - 1));
        entry_.store(head, std::memory_order_release);
    }

    ~ObjectPool() noexcept
    {
        assert(activeCount() == 0 && "Objects not returned to pool!");
    }

    T* acquire() noexcept
    {
        CountedNodePtr ptr = getCountedNode();
        return getObject(ptr);
    }

    void release(T* obj) noexcept
    {
        if (!obj)
            return;

        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            if (nodes_[i].obj == obj)
            {
                CountedNodePtr ptr(static_cast<int>(i));
                reclaim(ptr);
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
        size_t free = 0;
        CountedNodePtr current = entry_.load(std::memory_order_acquire);
        while (current.nodeIdx >= 0)
        {
            ++free;
            current = nodes_[current.nodeIdx].next;
        }
        return size() - free;
    }

    size_t freeCount() const noexcept
    {
        size_t free = 0;
        CountedNodePtr current = entry_.load(std::memory_order_acquire);
        while (current.nodeIdx >= 0)
        {
            ++free;
            current = nodes_[current.nodeIdx].next;
        }
        return free;
    }

    bool empty() const noexcept
    {
        return entry_.load(std::memory_order_acquire).nodeIdx < 0;
    }

private:
    void reclaim(CountedNodePtr nd) noexcept
    {
        if (nd.nodeIdx < 0)
            return;

        ++nd.externalCnt;

        CountedNodePtr oldHead = entry_.load(std::memory_order_acquire);
        do
        {
            nodes_[nd.nodeIdx].next = oldHead;
        } while (!entry_.compare_exchange_weak(oldHead, nd, std::memory_order_release, std::memory_order_acquire));
    }

    CountedNodePtr getCountedNode() noexcept
    {
        if (nodes_.empty())
            return CountedNodePtr();

        CountedNodePtr old = entry_.load(std::memory_order_acquire);
        while (true)
        {
            old = increaseEntryCnt(old);

            int nodeIdx = old.nodeIdx;
            if (nodeIdx < 0)
                break;

            Node* node = &nodes_[nodeIdx];
            CountedNodePtr next = node->next;

            if (entry_.compare_exchange_strong(old, next, std::memory_order_acquire, std::memory_order_relaxed))
            {
                return old;
            }
        }

        return CountedNodePtr();
    }

    T* getObject(CountedNodePtr& nodeCnt) noexcept
    {
        if (nodeCnt.nodeIdx < 0)
            return nullptr;
        return nodes_[nodeCnt.nodeIdx].obj;
    }

    CountedNodePtr increaseEntryCnt(CountedNodePtr& oldCnt) noexcept
    {
        CountedNodePtr newCnt;
        do
        {
            newCnt = oldCnt;
            ++newCnt.externalCnt;
        } while (
            !entry_.compare_exchange_strong(oldCnt, newCnt, std::memory_order_acquire, std::memory_order_relaxed));
        return newCnt;
    }

    std::vector<std::unique_ptr<T>> objs_;
    std::vector<Node> nodes_;
    alignas(64) std::atomic<CountedNodePtr> entry_;
};

} // namespace casket::lf