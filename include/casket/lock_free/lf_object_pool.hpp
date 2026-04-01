#pragma once
#include <array>
#include <memory>
#include <atomic>
#include <cstddef>
#include <casket/lock_free/lf_list_node.hpp>

namespace casket
{

template <class T, size_t N>
class ObjectPool
{
private:
    using Node = ListNode<T>;
    std::array<Node, N> storage_;
    std::atomic<Node*> free_list_{nullptr};
    std::atomic<size_t> acquired_{0};

public:
    ObjectPool()
    {
        for (size_t i = 0; i < N - 1; ++i)
        {
            storage_[i].next.store(&storage_[i + 1], std::memory_order_relaxed);
        }

        storage_[N - 1].next.store(nullptr, std::memory_order_relaxed);
        free_list_.store(&storage_[0], std::memory_order_relaxed);
    }

    Node* acquire() noexcept
    {
        Node* head = free_list_.load(std::memory_order_acquire);
        while (head)
        {
            Node* next = head->next.load(std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                acquired_.fetch_add(1, std::memory_order_relaxed);
                return head;
            }
        }
        return nullptr;
    }

    void release(Node* node) noexcept
    {
        if (!node)
            return;
        Node* head = free_list_.load(std::memory_order_relaxed);
        do
        {
            node->next.store(head, std::memory_order_relaxed);
        } while (!free_list_.compare_exchange_weak(head, node, std::memory_order_release, std::memory_order_relaxed));
        acquired_.fetch_sub(1, std::memory_order_relaxed);
    }

    size_t available() const noexcept
    {
        return storage_.size() - acquired_.load(std::memory_order_relaxed);
    }

    size_t capacity() const noexcept
    {
        return storage_.size();
    }
};

} // namespace casket