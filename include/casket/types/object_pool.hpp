#pragma once

#include <cstddef>
#include <cassert>
#include <new>

namespace casket
{

/// @brief Policy that always uses heap allocation
struct HeapAllocationPolicy
{
    template <typename Node, typename... Args>
    static Node* create(Args&&... args)
    {
        return new Node(std::forward<Args>(args)...);
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
    template <typename Node, typename... Args>
    static Node* create(Args&&...)
    {
        return nullptr;
    }

    template <typename Node>
    static void destroy(Node*)
    {
    }
};

template <typename T, typename AllocPolicy = HeapAllocationPolicy>
class ObjectPool
{
public:
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

    template <typename... Args>
    explicit ObjectPool(size_t poolSize, Args&&... args)
        : poolSize_(poolSize)
        , pool_(static_cast<char*>(operator new[](poolSize * sizeof(Node))))
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

    ~ObjectPool()
    {
        clear();
        operator delete[](pool_);
    }

    T* acquire()
    {
        if (freeList_)
        {
            Node* node = freeList_;
            freeList_ = freeList_->next;
            return &node->data;
        }
        
        Node* node = AllocPolicy::template create<Node>();
        return node ? &node->data : nullptr;
    }

    template <typename... Args>
    T* acquire(Args&&... args)
    {
        if (freeList_)
        {
            Node* node = freeList_;
            freeList_ = freeList_->next;
            new (&node->data) T(std::forward<Args>(args)...);
            return &node->data;
        }
        
        Node* node = AllocPolicy::template create<Node>(std::forward<Args>(args)...);
        return node ? &node->data : nullptr;
    }

    void release(T* data)
    {
        if (!data)
            return;

        Node* node = reinterpret_cast<Node*>(reinterpret_cast<char*>(data) - offsetof(Node, data));
        
        if (isFromPool(node))
        {
            node->next = freeList_;
            freeList_ = node;
        }
        else
        {
            data->~T();
            AllocPolicy::template destroy<Node>(node);
        }
    }

    size_t poolSize() const
    {
        return poolSize_;
    }

    void clear()
    {
        Node* node = freeList_;
        Node* prev = nullptr;

        while (node)
        {
            Node* next = node->next;

            if (!isFromPool(node))
            {
                if (prev)
                    prev->next = next;
                else
                    freeList_ = next;

                AllocPolicy::template destroy<Node>(node);
                node = next;
            }
            else
            {
                prev = node;
                node = next;
            }
        }
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

private:
    bool isFromPool(Node* node) const
    {
        const char* nodePtr = reinterpret_cast<const char*>(node);
        return nodePtr >= pool_ && nodePtr < pool_ + poolSize_ * sizeof(Node);
    }

    size_t poolSize_;
    char* pool_;
    Node* freeList_;
};

template <typename T>
using HeapObjectPool = ObjectPool<T, HeapAllocationPolicy>;

template <typename T>
using StrictObjectPool = ObjectPool<T, StrictHeapPolicy>;

} // namespace casket