#pragma once

#include <atomic>
#include <memory>
#include <vector>

namespace casket
{

/// @brief Lock-free MPSC queue with optional node pooling.
/// @tparam T Type of stored elements.
/// @note Multiple producers, single consumer. Thread-safe for push() from any thread.
///       pop() must be called from a single consumer thread.
template <typename T>
class MPSCQueue
{
private:
    /// @brief Internal node containing data and next pointer.
    struct Node
    {
        T data;                           ///< Stored element.
        std::atomic<Node*> next{nullptr}; ///< Pointer to next node.
    };

    /// @brief Node pool for memory reuse.
    /// @details Reduces dynamic allocations. Falls back to heap when pool is exhausted.
    class NodePool
    {
    public:
        /// @brief Constructs a node pool with fixed size.
        /// @param[in] poolSize Number of preallocated nodes. Default 8192.
        explicit NodePool(size_t poolSize = 8192)
            : pool_(poolSize)
        {
            for (size_t i = 0; i < poolSize - 1; ++i)
            {
                pool_[i].next.store(&pool_[i + 1], std::memory_order_relaxed);
            }
            pool_[poolSize - 1].next.store(nullptr, std::memory_order_relaxed);
            freeList_.store(&pool_[0], std::memory_order_relaxed);
        }

        /// @brief Acquires a node from the pool or heap.
        /// @return Pointer to a usable node.
        Node* acquire()
        {
            Node* node = freeList_.load(std::memory_order_acquire);
            while (node)
            {
                Node* next = node->next.load(std::memory_order_relaxed);
                if (freeList_.compare_exchange_weak(node, next, std::memory_order_acq_rel, std::memory_order_relaxed))
                {
                    return node;
                }
                node = freeList_.load(std::memory_order_acquire);
            }
            return new Node();
        }

        /// @brief Returns a node back to the pool.
        /// @param[in] node Node to release. Can be nullptr.
        void release(Node* node)
        {
            if (!node)
                return;

            Node* oldHead = freeList_.load(std::memory_order_relaxed);
            do
            {
                node->next.store(oldHead, std::memory_order_relaxed);
            } while (
                !freeList_.compare_exchange_weak(oldHead, node, std::memory_order_release, std::memory_order_relaxed));
        }

        /// @brief Destructor. Frees heap-allocated nodes.
        ~NodePool()
        {
            Node* node = freeList_.load();
            while (node)
            {
                Node* next = node->next.load();
                if (node >= pool_.data() && node < pool_.data() + pool_.size())
                {
                    // From pool - do not delete
                }
                else
                {
                    delete node;
                }
                node = next;
            }
        }

    private:
        std::vector<Node> pool_;      ///< Preallocated storage.
        std::atomic<Node*> freeList_; ///< Lock-free free list head.
    };

public:
    /// @brief Constructs an MPSC queue.
    /// @param[in] poolSize Size of internal node pool. Default 8192.
    explicit MPSCQueue(size_t poolSize = 8192)
        : pool_(poolSize)
    {
        Node* stub = pool_.acquire();
        stub->next.store(nullptr, std::memory_order_relaxed);
        head_.store(stub, std::memory_order_relaxed);
        tail_.store(stub, std::memory_order_relaxed);
    }

    /// @brief Destructor. Clears all elements and releases resources.
    ~MPSCQueue() noexcept
    {
        clear();
        pool_.release(head_.load());
    }

    /// @brief Pushes an element into the queue.
    /// @param[in] value Rvalue reference to element. Will be moved.
    void push(T&& value)
    {
        Node* node = pool_.acquire();
        node->data = std::move(value);
        node->next.store(nullptr, std::memory_order_relaxed);

        Node* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    /// @brief Pushes an element into the queue (copy version).
    /// @param[in] value Const lvalue reference to element. Will be copied.
    void push(const T& value)
    {
        Node* node = pool_.acquire();
        node->data = value;  // Copy
        node->next.store(nullptr, std::memory_order_relaxed);

        Node* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    /// @brief Pops an element from the queue.
    /// @param[out] value Output parameter for the popped element.
    /// @return true if an element was popped, false if queue was empty.
    /// @note Single consumer only. Must be called from one thread.
    bool pop(T& value)
    {
        Node* tail = tail_.load(std::memory_order_acquire);
        Node* next = tail->next.load(std::memory_order_acquire);

        if (next == nullptr)
        {
            return false;
        }

        value = std::move(next->data);
        tail_.store(next, std::memory_order_release);

        pool_.release(tail);
        return true;
    }

    /// @brief Checks whether the queue is empty.
    /// @return true if empty, false otherwise.
    /// @note Intended for single consumer use. May be stale if producers are active.
    bool empty() const
    {
        Node* tail = tail_.load(std::memory_order_acquire);
        return tail->next.load(std::memory_order_acquire) == nullptr;
    }

    /// @brief Removes all elements from the queue.
    /// @note Single consumer only.
    void clear() noexcept
    {
        T dummy;
        while (pop(dummy)) {}
    }

private:
    NodePool pool_;                       ///< Node memory manager.
    alignas(64) std::atomic<Node*> head_; ///< Producer-side pointer.
    alignas(64) std::atomic<Node*> tail_; ///< Consumer-side pointer.
};

} // namespace casket