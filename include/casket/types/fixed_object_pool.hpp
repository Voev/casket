#pragma once

#include <cstddef>
#include <cassert>
#include <new>
#include <type_traits>
#include <casket/utils/container_of.hpp>

namespace casket
{

template <typename T>
class FixedObjectPool final
{
private:
    template <typename U>
    static auto has_reset(int) -> decltype(std::declval<U>().reset(), std::true_type{});

    template <typename>
    static std::false_type has_reset(...);

    static constexpr bool has_reset_method = decltype(has_reset<T>(0))::value;

public:
    struct Node
    {
        T data;
        Node* next{nullptr};
        uint32_t generation{0};

        template <typename... Args>
        Node(Args&&... args)
            : data(std::forward<Args>(args)...)
            , next(nullptr)
        {
        }
    };

    template <typename... Args>
    explicit FixedObjectPool(size_t poolSize, Args&&... args)
        : poolSize_(poolSize)
        , poolGeneration_(0)
        , pool_(static_cast<char*>(::operator new[](poolSize * sizeof(Node))))
        , freeList_(nullptr)
    {
        char* ptr = pool_;
        for (size_t i = 0; i < poolSize; ++i)
        {
            Node* node = new (ptr) Node(std::forward<Args>(args)...);
            node->next = freeList_;
            freeList_ = node;
            ptr += sizeof(Node);
        }
    }

    ~FixedObjectPool() noexcept
    {
        char* ptr = pool_;

        for (size_t i = 0; i < poolSize_; ++i)
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

    FixedObjectPool(FixedObjectPool&& rhs) noexcept
        : poolSize_(rhs.poolSize_)
        , poolGeneration_(rhs.poolGeneration_)
        , pool_(rhs.pool_)
        , freeList_(rhs.freeList_)
    {
        rhs.poolSize_ = 0;
        rhs.poolGeneration_ = 0;
        rhs.pool_ = nullptr;
        rhs.freeList_ = nullptr;
    }

    FixedObjectPool& operator=(FixedObjectPool&& rhs) noexcept
    {
        if (this != &rhs)
        {
            this->~FixedObjectPool();
            new (this) FixedObjectPool(std::move(rhs));
        }
        return *this;
    }

    T* acquire()
    {
        if (!freeList_)
            return nullptr;

        Node* node = freeList_;
        freeList_ = freeList_->next;
        return &node->data;
    }

    template <typename... Args>
    T* acquire(Args&&... args)
    {
        if (!freeList_)
            return nullptr;

        Node* node = freeList_;
        freeList_ = freeList_->next;
        node->data.~T();
        new (&node->data) T(std::forward<Args>(args)...);
        return &node->data;
    }

    void release(T* data) noexcept
    {
        if (!data)
        {
            return;
        }

        Node* node = container_of(data, &Node::data);
        if (isFromPool(node) && node->generation == poolGeneration_)
        {
            node->next = freeList_;
            freeList_ = node;
        }
    }

    size_t poolSize() const noexcept
    {
        return poolSize_;
    }

    size_t capacity() const noexcept
    {
        return poolSize_;
    }

    void reset()
    {
        if (!pool_)
        {
            return;
        }

        freeList_ = nullptr;
        poolGeneration_++;

        char* ptr = pool_;
        for (size_t i = 0; i < poolSize_; ++i)
        {
            Node* node = reinterpret_cast<Node*>(ptr);

            if constexpr (has_reset_method)
            {
                node->data.reset();
            }

            node->generation = poolGeneration_;
            node->next = freeList_;
            freeList_ = node;
            ptr += sizeof(Node);
        }
    }

private:
    bool isFromPool(Node* node) const noexcept
    {
        const char* nodePtr = reinterpret_cast<const char*>(node);
        return nodePtr >= pool_ && nodePtr < pool_ + poolSize_ * sizeof(Node);
    }

    size_t poolSize_;
    uint32_t poolGeneration_;
    char* pool_;
    Node* freeList_;
};

} // namespace casket