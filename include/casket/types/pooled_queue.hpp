#pragma once

#include <cstddef>
#include <utility>
#include <casket/types/fixed_object_pool.hpp>

namespace casket
{

/// @brief Single-threaded queue with optional node pooling.
/// @tparam T Type of stored elements.
/// @tparam PoolType Pool type for node.
/// @note NOT thread-safe. All operations must be called from a single thread.
template <typename T, template <typename> class PoolType = FixedObjectPool>
class PooledQueue
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

    using NodePool = PoolType<Node>;

public:
    /// @brief Constructs an empty queue.
    /// @param poolSize Size of internal node pool.
    /// @param args Arguments to initialize all nodes in the pool (including stub)
    template <typename... Args>
    explicit PooledQueue(size_t poolSize, Args&&... args)
        : pool_(poolSize, std::forward<Args>(args)...)
    {
    }

    /// @brief Destructor. Clears all elements and releases resources.
    ~PooledQueue() noexcept
    {
        clear();
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

        if (empty())
        {
            head_ = node;
            tail_ = node;
        }
        else
        {
            tail_->next = node;
            tail_ = node;
        }
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
        
        if (empty())
        {
            head_ = node;
            tail_ = node;
        }
        else
        {
            tail_->next = node;
            tail_ = node;
        }
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
        
        if (empty())
        {
            head_ = node;
            tail_ = node;
        }
        else
        {
            tail_->next = node;
            tail_ = node;
        }
        return true;
    }

    /// @brief Pops an element from the queue (without returning value).
    void pop()
    {
        if (empty())
        {
            return;
        }

        Node* toRemove = head_;
        head_ = head_->next;

        if (head_ == nullptr)
        {
            tail_ = nullptr;
        }

        pool_.release(toRemove);
    }

    /// @brief Returns the front element without removing it.
    /// @return Reference to the front element.
    /// @pre Queue must not be empty.
    T& front()
    {
        assert(head_);
        return head_->data;
    }

    /// @brief Returns the front element without removing it (const version).
    /// @return Const reference to the front element.
    /// @pre Queue must not be empty.
    const T& front() const
    {
        assert(head_);
        return head_->data;
    }

    /// @brief Checks whether the queue is empty.
    /// @return true if empty, false otherwise.
    bool empty() const
    {
        return (head_ == nullptr);
    }

    /// @brief Returns the number of elements in the queue.
    /// @note O(n) complexity.
    size_t size() const
    {
        size_t count = 0;
        Node* current = head_;
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
        while (!empty())
        {
            pop();
        }
    }

    /// @brief Returns statistics about pool usage.
    /// @return Pair of (pool_size, used_nodes_count)
    std::pair<size_t, size_t> poolStats() const
    {
        return {pool_.poolSize(), size()};
    }

private:
    NodePool pool_;
    Node* head_{nullptr}; ///< Points to the last node (producer side)
    Node* tail_{nullptr}; ///< Points to the dummy node before first element
};

} // namespace casket