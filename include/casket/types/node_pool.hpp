#pragma once

#include <vector>
#include <cassert>
#include <type_traits>
#include <new>

namespace casket
{

/// @brief Policy that always uses heap allocation
struct HeapAllocationPolicy
{
    template <typename Node>
    static Node* create()
    {
        return new Node();
    }

    template <typename Node>
    static void destroy(Node* node)
    {
        delete node;
    }
};

/// @brief Policy that never uses heap, only preallocated pool
struct StrictHeapPolicy
{
    template <typename Node>
    static Node* create()
    {
        return nullptr; // Will be handled by pool
    }

    template <typename Node>
    static void destroy(Node*)
    {
        // Do nothing - node returns to pool
    }
};

/// @brief Single-threaded node pool with configurable allocation policy
/// @tparam T Type of stored elements
/// @tparam AllocPolicy Allocation policy (HeapAllocationPolicy, PoolWithFallbackPolicy, etc.)
/// @note NOT thread-safe. Designed for single-threaded use only.
template <typename T, typename AllocPolicy = HeapAllocationPolicy>
class NodePool
{
public:
    struct Node
    {
        T data;
        Node* next{nullptr};

        template <typename... Args>
        Node(Args&&... args)
            : data(std::forward<Args>(args)...)
        {
        }
    };

    /// @brief Constructs a node pool with fixed size.
    /// @param[in] poolSize Number of preallocated nodes. Default 8192.
    explicit NodePool(size_t poolSize = 8192)
        : pool_(poolSize)
        , freeList_(nullptr)
    {
        // Initialize free list
        for (size_t i = 0; i < poolSize; ++i)
        {
            // Placement new to construct nodes in preallocated memory
            new (&pool_[i]) Node();
            pool_[i].next = freeList_;
            freeList_ = &pool_[i];
        }
    }

    /// @brief Destructor
    ~NodePool()
    {
        clear();
    }

    /// @brief Acquires a node from the pool or heap based on policy
    /// @return Pointer to a usable node
    Node* acquire()
    {
        if (freeList_)
        {
            Node* node = freeList_;
            freeList_ = freeList_->next;
            return node;
        }

        // Pool exhausted - use allocation policy
        return allocateFromPolicy();
    }

    /// @brief Acquires a node with constructed data
    /// @param[in] args Arguments to forward to Node constructor
    /// @return Pointer to a usable node with constructed data
    template <typename... Args>
    Node* acquire(Args&&... args)
    {
        Node* node = acquire();
        if (node)
        {
            // Call placement new to construct data
            new (&node->data) T(std::forward<Args>(args)...);
        }
        return node;
    }

    /// @brief Returns a node back to the pool
    /// @param[in] node Node to release. Can be nullptr.
    void release(Node* node)
    {
        if (!node)
            return;

        // Call destructor on data
        node->data.~T();

        // Check if node belongs to pool
        if (isFromPool(node))
        {
            node->next = freeList_;
            freeList_ = node;
        }
        else
        {
            // Node from heap - use policy to destroy
            destroyFromPolicy(node);
        }
    }

    /// @brief Returns the number of nodes currently in the pool (free + used)
    size_t poolSize() const
    {
        return pool_.size();
    }

    /// @brief Clears all heap-allocated nodes from the free list
    void clear()
    {
        // Destroy all heap nodes in free list
        Node* node = freeList_;
        Node* prev = nullptr;

        while (node)
        {
            Node* next = node->next;

            if (!isFromPool(node))
            {
                // Remove from free list
                if (prev)
                    prev->next = next;
                else
                    freeList_ = next;

                destroyFromPolicy(node);
                node = next;
            }
            else
            {
                prev = node;
                node = next;
            }
        }
    }

    // Disable copy/move
    NodePool(const NodePool&) = delete;
    NodePool& operator=(const NodePool&) = delete;

private:
    bool isFromPool(Node* node) const
    {
        const char* poolStart = reinterpret_cast<const char*>(pool_.data());
        const char* poolEnd = poolStart + pool_.size() * sizeof(Node);
        const char* nodePtr = reinterpret_cast<const char*>(node);

        return nodePtr >= poolStart && nodePtr < poolEnd;
    }

    Node* allocateFromPolicy()
    {
        return AllocPolicy::template create<Node>();
    }

    void destroyFromPolicy(Node* node)
    {
        AllocPolicy::template destroy<Node>(node);
    }

    std::vector<Node> pool_;
    Node* freeList_;
};

/// @brief Convenience alias for pool with heap fallback (default behavior)
template <typename T>
using HeapNodePool = NodePool<T, HeapAllocationPolicy>;

/// @brief Convenience alias for pool-only (no heap, but asserts on exhaustion)
template <typename T>
using StrictNodePool = NodePool<T, StrictHeapPolicy>;

} // namespace casket