#pragma once

#include <cstddef>
#include <utility>
#include <casket/types/object_pool.hpp>

namespace casket
{

/// @brief Single-threaded queue with optional node pooling.
/// @tparam T Type of stored elements.
/// @tparam AllocPolicy Allocation policy for node pool.
/// @note NOT thread-safe. All operations must be called from a single thread.
template <typename T, typename AllocPolicy = HeapAllocationPolicy>
class BasicPooledQueue
{
private:
    struct Node
    {
        T data;
        Node* next{nullptr};

        template <typename... Args>
        Node(Args&&... args)
            : data(std::forward<Args>(args)...)
            , next(nullptr)
        {
        }
    };

    using PoolType = ObjectPool<Node, AllocPolicy>;

public:
    /// @brief Constructs an empty queue.
    /// @param poolSize Size of internal node pool.
    /// @param args Arguments to initialize all nodes in the pool (including stub)
    template <typename... Args>
    explicit BasicPooledQueue(size_t poolSize, Args&&... args)
        : pool_(poolSize + 1, std::forward<Args>(args)...)
    {
        Node* stub = pool_.acquire();
        if (stub)
        {
            stub->next = nullptr;
            head_ = stub;
            tail_ = stub;
        }
    }

    /// @brief Destructor. Clears all elements and releases resources.
    ~BasicPooledQueue() noexcept
    {
        clear();
        pool_.release(head_);
    }

    /// @brief Pushes an element into the queue (move version).
    /// @param value Rvalue reference to element. Will be moved.
    bool push(T&& value)
    {
        Node* node = pool_.acquire(std::move(value));
        if (!node)
        {
            return false;
        }

        node->next = nullptr;
        head_->next = node;
        head_ = node;
        return true;
    }

    /// @brief Pushes an element into the queue (copy version).
    /// @param value Const lvalue reference to element. Will be copied.
    bool push(const T& value)
    {
        Node* node = pool_.acquire(value);
        if (!node)
        {
            return false;
        }

        node->next = nullptr;
        head_->next = node;
        head_ = node;
        return true;
    }

    /// @brief Pushes an element with emplacement.
    /// @param args Arguments to forward to T's constructor.
    template <typename... Args>
    bool emplace(Args&&... args)
    {
        Node* node = pool_.acquire(std::forward<Args>(args)...);
        if (!node)
        {
            return false;
        }

        node->next = nullptr;
        head_->next = node;
        head_ = node;
        return true;
    }

    /// @brief Pops an element from the queue (without returning value).
    void pop()
    {
        Node* next = tail_->next;
        if (next == nullptr)
        {
            return;
        }

        pool_.release(tail_);
        tail_ = next;
    }

    /// @brief Returns the front element without removing it.
    /// @return Reference to the front element.
    /// @pre Queue must not be empty.
    T& front()
    {
        assert(tail_);
        return tail_->next->data;
    }

    /// @brief Returns the front element without removing it (const version).
    /// @return Const reference to the front element.
    /// @pre Queue must not be empty.
    const T& front() const
    {
        assert(tail_);
        return tail_->next->data;
    }

    /// @brief Checks whether the queue is empty.
    /// @return true if empty, false otherwise.
    bool empty() const
    {
        return (tail_ == nullptr) || (tail_->next == nullptr);
    }

    /// @brief Returns the number of elements in the queue.
    /// @note O(n) complexity.
    size_t size() const
    {
        size_t count = 0;
        if (tail_)
        {
            Node* current = tail_->next;
            while (current)
            {
                ++count;
                current = current->next;
            }
        }
        return count;
    }

    /// @brief Removes all elements from the queue.
    void clear() noexcept
    {
        while (tail_->next)
        {
            Node* next = tail_->next;
            pool_.release(tail_);
            tail_ = next;
        }
    }

    /// @brief Swaps the contents with another queue.
    /// @param other Queue to swap with.
    void swap(BasicPooledQueue& other) noexcept
    {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
    }

    /// @brief Returns statistics about pool usage.
    /// @return Pair of (pool_size, used_nodes_count)
    std::pair<size_t, size_t> poolStats() const
    {
        return {pool_.poolSize() - 1, size()};
    }

private:
    PoolType pool_;
    Node* head_{nullptr}; ///< Points to the last node (producer side)
    Node* tail_{nullptr}; ///< Points to the dummy node before first element
};

/// @brief Convenience alias for default heap-fallback policy
template <typename T>
using ExpandPooledQueue = BasicPooledQueue<T, HeapAllocationPolicy>;

/// @brief Convenience alias for no-heap policy
template <typename T>
using StrictPooledQueue = BasicPooledQueue<T, StrictHeapPolicy>;

/// @brief Swaps two queues.
template <typename T, typename AllocPolicy>
void swap(BasicPooledQueue<T, AllocPolicy>& lhs, BasicPooledQueue<T, AllocPolicy>& rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace casket