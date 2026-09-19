#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <new>
#include <type_traits>
#include <casket/utils/container_of.hpp>

namespace casket
{

/// @brief Fixed-size object pool with in-place construction and reuse.
///
/// @details
/// `FixedObjectPool<T>` preallocates a contiguous block of `capacity` nodes,
/// each holding a `T` instance plus bookkeeping (intrusive free list link,
/// generation counter and an `inUse` flag). Objects are handed out and
/// returned in O(1) without any dynamic allocation after construction.
///
/// The pool is non-copyable but movable. After a move, the source pool is
/// left in an empty, valid state.
///
/// @tparam T Type of the pooled objects. If `T` has a `reset()` member,
///           it will be invoked for every node during `reset()`.
///
/// @note Not thread-safe. External synchronization is required if the pool
///       is shared across threads.
template <typename T>
class FixedObjectPool final
{
private:
    /// @brief SFINAE probe: detects whether `U` has a `reset()` member.
    template <typename U>
    static auto has_reset(int) -> decltype(std::declval<U>().reset(), std::true_type{});

    /// @brief SFINAE fallback: `U` has no `reset()` member.
    template <typename>
    static std::false_type has_reset(...);

    /// @brief True if `T` exposes a `reset()` method.
    static constexpr bool has_reset_method = decltype(has_reset<T>(0))::value;

public:
    /// @brief Internal storage node.
    ///
    /// @details Holds the pooled object together with metadata used for
    /// free-list linkage, generation tracking and double-release detection.
    struct Node
    {
        /// @brief The pooled object.
        T data;

        /// @brief Next node in the free list (intrusive link).
        Node* next{nullptr};

        /// @brief Generation marker; used to invalidate stale pointers after `reset()`.
        uint32_t generation{0};

        /// @brief True while the node is handed out to a caller.
        ///
        /// @details Guards against double `release()` within the same generation.
        bool inUse{false};

        /// @brief Constructs the node's payload in place.
        /// @param args Arguments forwarded to `T`'s constructor.
        template <typename... Args>
        Node(Args&&... args)
            : data(std::forward<Args>(args)...)
            , next(nullptr)
            , generation(0)
            , inUse(false)
        {
        }
    };

    /// @brief Constructs the pool and preallocates `capacity` nodes.
    ///
    /// @param capacity Maximum number of objects the pool can hold.
    /// @param args     Arguments forwarded to every `T` instance's constructor.
    ///
    /// @note All nodes are constructed eagerly, even before `acquire()`.
    template <typename... Args>
    explicit FixedObjectPool(size_t capacity, Args&&... args)
        : capacity_(capacity)
        , used_(0)
        , poolGeneration_(0)
        , pool_(static_cast<char*>(::operator new[](capacity * sizeof(Node))))
        , freeList_(nullptr)
    {
        char* ptr = pool_;
        for (size_t i = 0; i < capacity_; ++i)
        {
            Node* node = new (ptr) Node(std::forward<Args>(args)...);
            node->next = freeList_;
            node->inUse = false;
            freeList_ = node;
            ptr += sizeof(Node);
        }
    }

    /// @brief Destroys all pooled objects and releases the backing storage.
    ~FixedObjectPool() noexcept
    {
        if (!pool_)
            return;

        char* ptr = pool_;

        for (size_t i = 0; i < capacity_; ++i)
        {
            Node* node = reinterpret_cast<Node*>(ptr);

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                node->data.~T();
            }

            ptr += sizeof(Node);
        }

        ::operator delete[](pool_);
    }

    FixedObjectPool(const FixedObjectPool&) = delete;
    FixedObjectPool& operator=(const FixedObjectPool&) = delete;

    /// @brief Move-constructs the pool, taking ownership of `rhs`'s storage.
    /// @param rhs Pool to move from. Left empty and valid.
    FixedObjectPool(FixedObjectPool&& rhs) noexcept
        : capacity_(rhs.capacity_)
        , used_(rhs.used_)
        , poolGeneration_(rhs.poolGeneration_)
        , pool_(rhs.pool_)
        , freeList_(rhs.freeList_)
    {
        rhs.capacity_ = 0;
        rhs.used_ = 0;
        rhs.poolGeneration_ = 0;
        rhs.pool_ = nullptr;
        rhs.freeList_ = nullptr;
    }

    /// @brief Move-assigns the pool.
    /// @param rhs Pool to move from. Left empty and valid.
    /// @return `*this`.
    FixedObjectPool& operator=(FixedObjectPool&& rhs) noexcept
    {
        if (this != &rhs)
        {
            this->~FixedObjectPool();
            new (this) FixedObjectPool(std::move(rhs));
        }
        return *this;
    }

    /// @brief Acquires an object from the pool without reinitializing it.
    ///
    /// @details The returned object retains whatever state it had when it was
    /// last released (or its initial constructed state). Prefer the
    /// variadic overload if you need a freshly constructed payload.
    ///
    /// @return Pointer to the pooled object, or `nullptr` if the pool is exhausted.
    T* acquire()
    {
        if (!freeList_)
            return nullptr;

        Node* node = freeList_;
        freeList_ = freeList_->next;

        node->inUse = true;
        ++used_;

        return &node->data;
    }

    /// @brief Acquires an object, destroying the old payload and constructing a new one.
    ///
    /// @param args Arguments forwarded to `T`'s constructor.
    /// @return Pointer to the newly constructed object, or `nullptr` if the pool is exhausted.
    template <typename... Args>
    T* acquire(Args&&... args)
    {
        if (!freeList_)
            return nullptr;

        Node* node = freeList_;
        freeList_ = freeList_->next;

        node->data.~T();
        new (&node->data) T(std::forward<Args>(args)...);

        node->inUse = true;
        ++used_;

        return &node->data;
    }

    /// @brief Returns an object to the pool.
    ///
    /// @details The call is a no-op if:
    ///          - `data` is `nullptr`,
    ///          - `data` does not belong to this pool,
    ///          - the node's generation is stale (i.e. `reset()` was called after acquire),
    ///          - or the node is not currently marked as in-use (double release).
    ///
    /// @param data Pointer previously returned by `acquire()`. May be `nullptr`.
    void release(T* data) noexcept
    {
        if (!data)
            return;

        Node* node = container_of(data, &Node::data);

        if (isFromPool(node) && node->generation == poolGeneration_ && node->inUse)
        {
            node->inUse = false;
            node->next = freeList_;
            freeList_ = node;
            --used_;
        }
    }

    /// @brief Number of objects currently handed out (in use).
    /// @return Occupied slot count.
    size_t poolSize() const noexcept
    {
        return used_;
    }

    /// @brief Maximum number of objects the pool can hold.
    /// @return Total slot count configured at construction.
    size_t capacity() const noexcept
    {
        return capacity_;
    }

    /// @brief Number of free slots available for `acquire()`.
    /// @return `capacity() - poolSize()`.
    size_t available() const noexcept
    {
        return capacity_ - used_;
    }

    /// @brief Returns all objects to the pool and invalidates stale pointers.
    ///
    /// @details
    /// - Increments the pool generation, so any pointer acquired before this
    ///   call can no longer be released back into the pool.
    /// - Clears the `inUse` flag on every node.
    /// - Calls `T::reset()` on each payload if `T` provides such a method.
    ///
    /// If the pool has been moved from, this is a no-op.
    void reset()
    {
        if (!pool_)
            return;

        freeList_ = nullptr;
        used_ = 0;
        poolGeneration_++;

        char* ptr = pool_;
        for (size_t i = 0; i < capacity_; ++i)
        {
            Node* node = reinterpret_cast<Node*>(ptr);

            if constexpr (has_reset_method)
            {
                node->data.reset();
            }

            node->generation = poolGeneration_;
            node->inUse = false;
            node->next = freeList_;
            freeList_ = node;
            ptr += sizeof(Node);
        }
    }

private:
    /// @brief Checks whether `node` lies within this pool's backing storage.
    /// @param node Node pointer to test.
    /// @return `true` if `node` belongs to this pool.
    bool isFromPool(Node* node) const noexcept
    {
        if (!pool_)
            return false;

        const char* nodePtr = reinterpret_cast<const char*>(node);
        return nodePtr >= pool_ && nodePtr < pool_ + capacity_ * sizeof(Node);
    }

    /// @brief Maximum number of objects the pool can hold.
    size_t capacity_;

    /// @brief Number of objects currently handed out.
    size_t used_;

    /// @brief Monotonic generation counter, bumped on every `reset()`.
    uint32_t poolGeneration_;

    /// @brief Raw backing storage for all nodes.
    char* pool_;

    /// @brief Head of the intrusive free list.
    Node* freeList_;
};

} // namespace casket