#pragma once

#include <cstddef>
#include <casket/types/node_pool.hpp>

namespace casket
{

/// @brief Single-threaded queue with optional node pooling.
/// @tparam T Type of stored elements.
/// @tparam AllocPolicy Allocation policy for node pool.
/// @note NOT thread-safe. All operations must be called from a single thread.
template <typename T, typename AllocPolicy = HeapAllocationPolicy>
class PooledQueue
{
private:
    using Node = typename NodePool<T, AllocPolicy>::Node;

public:
    /// @brief Constructs an empty queue.
    /// @param[in] poolSize Size of internal node pool. Default 8192.
    explicit PooledQueue(size_t poolSize = 8192)
        : pool_(poolSize)
    {
        Node* stub = pool_.acquire();
        stub->next = nullptr;
        head_ = stub;
        tail_ = stub;
    }

    /// @brief Destructor. Clears all elements and releases resources.
    ~PooledQueue() noexcept
    {
        clear();
        pool_.release(head_);
    }

    /// @brief Pushes an element into the queue (move version).
    /// @param[in] value Rvalue reference to element. Will be moved.
    void push(T&& value)
    {
        Node* node = pool_.acquire();
        node->data = std::move(value);
        node->next = nullptr;

        head_->next = node;
        head_ = node;
    }

    /// @brief Pushes an element into the queue (copy version).
    /// @param[in] value Const lvalue reference to element. Will be copied.
    void push(const T& value)
    {
        Node* node = pool_.acquire();
        node->data = value;  // Copy
        node->next = nullptr;

        head_->next = node;
        head_ = node;
    }

    /// @brief Pushes an element with emplacement.
    /// @param[in] args Arguments to forward to T's constructor.
    template <typename... Args>
    void emplace(Args&&... args)
    {
        Node* node = pool_.acquire(std::forward<Args>(args)...);
        node->next = nullptr;

        head_->next = node;
        head_ = node;
    }

    /// @brief Pops an element from the queue.
    /// @param[out] value Output parameter for the popped element.
    /// @return true if an element was popped, false if queue was empty.
    bool pop(T& value)
    {
        Node* next = tail_->next;
        
        if (next == nullptr)
        {
            return false;
        }

        value = std::move(next->data);
        pool_.release(tail_);
        tail_ = next;
        
        return true;
    }

    /// @brief Returns the front element without removing it.
    /// @return Reference to the front element.
    /// @pre Queue must not be empty.
    T& front()
    {
        return tail_->next->data;
    }

    /// @brief Returns the front element without removing it (const version).
    /// @return Const reference to the front element.
    /// @pre Queue must not be empty.
    const T& front() const
    {
        return tail_->next->data;
    }

    /// @brief Checks whether the queue is empty.
    /// @return true if empty, false otherwise.
    bool empty() const
    {
        return tail_->next == nullptr;
    }

    /// @brief Returns the number of elements in the queue.
    /// @note O(n) complexity.
    size_t size() const
    {
        size_t count = 0;
        Node* current = tail_->next;
        while (current)
        {
            ++count;
            current = current->next;
        }
        return count;
    }

    /// @brief Removes all elements from the queue.
    void clear() noexcept
    {
        T dummy;
        while (pop(dummy)) {}
    }

    /// @brief Swaps the contents with another queue.
    /// @param[in] other Queue to swap with.
    void swap(PooledQueue& other) noexcept
    {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        // Note: pools cannot be swapped as they may have different sizes
    }

    /// @brief Returns statistics about pool usage.
    /// @return Pair of (pool_size, heap_nodes_count)
    std::pair<size_t, size_t> pool_stats() const
    {
        return {pool_.poolSize(), pool_.heapNodesCount()};
    }

private:
    NodePool<T, AllocPolicy> pool_;
    Node* head_;  ///< Points to the last node (producer side)
    Node* tail_;  ///< Points to the dummy node before first element
};

/// @brief Convenience alias for default heap-fallback policy
template <typename T>
using HeapPooledQueue = PooledQueue<T, HeapAllocationPolicy>;

/// @brief Convenience alias for no-heap policy
template <typename T>
using StrictPooledQueue = PooledQueue<T, StrictHeapPolicy>;

/// @brief Swaps two queues.
template <typename T, typename AllocPolicy>
void swap(PooledQueue<T, AllocPolicy>& lhs, PooledQueue<T, AllocPolicy>& rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace casket