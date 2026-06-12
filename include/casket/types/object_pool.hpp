#pragma once

#include <cstddef>
#include <cassert>
#include <new>
#include <type_traits>

namespace casket
{

/// @brief Policy that allocates heap chunks (expands pool in blocks)
struct HeapAllocationPolicy
{
    struct Chunk
    {
        char* pool;
        size_t size;
        Chunk* next;
    };

    template <typename Node, typename... Args>
    static Node* create(Args&&... args)
    {
        // This policy works through ObjectPool's expansion mechanism
        // Direct single-node allocation is not used
        return nullptr;
    }

    template <typename Node>
    static void destroy(Node* node)
    {
        // Individual nodes are not destroyed directly
        // Chunks are destroyed as a whole
        (void)node;
    }

    template <typename Node>
    static Chunk* allocateChunk(size_t chunkSize)
    {
        Chunk* chunk = static_cast<Chunk*>(::operator new(sizeof(Chunk)));
        chunk->pool = static_cast<char*>(::operator new[](chunkSize * sizeof(Node)));
        chunk->size = chunkSize;
        chunk->next = nullptr;
        return chunk;
    }

    template <typename Node>
    static void deallocateChunk(Chunk* chunk)
    {
        ::operator delete[](chunk->pool);
        ::operator delete(chunk);
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
        , totalCapacity_(poolSize)
        , freeList_(nullptr)
        , chunks_(nullptr)
    {
        allocateChunk(poolSize, std::forward<Args>(args)...);
    }

    ~ObjectPool()
    {
        clear();
        freeAllChunks();
    }

    T* acquire()
    {
        if (freeList_)
        {
            Node* node = freeList_;
            freeList_ = freeList_->next;
            return &node->data;
        }
        
        // Try to expand pool if policy allows
        expandIfNeeded();
        if (freeList_)
        {
            Node* node = freeList_;
            freeList_ = freeList_->next;
            return &node->data;
        }
        
        return nullptr;
    }

    template <typename... Args>
    T* acquire(Args&&... args)
    {
        if (freeList_)
        {
            Node* node = freeList_;
            freeList_ = freeList_->next;
            
            // Need to call destructor first if object already exists
            // But in our pool, nodes are constructed with default args
            // So we should placement-new over existing memory
            node->data.~T();
            new (&node->data) T(std::forward<Args>(args)...);
            return &node->data;
        }
        
        // Try to expand pool if policy allows
        expandIfNeeded(std::forward<Args>(args)...);
        if (freeList_)
        {
            Node* node = freeList_;
            freeList_ = freeList_->next;
            new (&node->data) T(std::forward<Args>(args)...);
            return &node->data;
        }
        
        return nullptr;
    }

    void release(T* data)
    {
        if (!data)
            return;

        Node* node = reinterpret_cast<Node*>(reinterpret_cast<char*>(data) - offsetof(Node, data));
        
        if (isFromAnyChunk(node))
        {
            node->next = freeList_;
            freeList_ = node;
        }
    }

    size_t poolSize() const
    {
        return poolSize_;
    }

    size_t capacity() const
    {
        return totalCapacity_;
    }

    void clear()
    {
        // Only reset free list, but keep all nodes
        freeList_ = nullptr;
        
        // Rebuild free list from all chunks
        auto* chunk = chunks_;
        while (chunk)
        {
            char* ptr = chunk->pool;
            for (size_t i = 0; i < chunk->size; ++i)
            {
                Node* node = reinterpret_cast<Node*>(ptr);
                node->next = freeList_;
                freeList_ = node;
                ptr += sizeof(Node);
            }
            chunk = chunk->next;
        }
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

private:
    template <typename... Args>
    void allocateChunk(size_t size, Args&&... args)
    {
        if constexpr (std::is_same_v<AllocPolicy, HeapAllocationPolicy>)
        {
            auto* chunk = AllocPolicy::template allocateChunk<Node>(size);
            chunk->next = chunks_;
            chunks_ = chunk;
            
            char* ptr = chunk->pool;
            for (size_t i = 0; i < size; ++i)
            {
                Node* node = new (ptr) Node(std::forward<Args>(args)...);
                node->next = freeList_;
                freeList_ = node;
                ptr += sizeof(Node);
            }
        }
    }

    template <typename... Args>
    void expandIfNeeded(Args&&... args)
    {
        if constexpr (std::is_same_v<AllocPolicy, HeapAllocationPolicy>)
        {
            if (!freeList_)
            {
                size_t newChunkSize = poolSize_;
                allocateChunk(newChunkSize, std::forward<Args>(args)...);
                totalCapacity_ += newChunkSize;
            }
        }
    }

    bool isFromAnyChunk(Node* node) const
    {
        auto* chunk = chunks_;
        while (chunk)
        {
            const char* nodePtr = reinterpret_cast<const char*>(node);
            const char* poolStart = chunk->pool;
            const char* poolEnd = chunk->pool + chunk->size * sizeof(Node);
            
            if (nodePtr >= poolStart && nodePtr < poolEnd)
                return true;
            
            chunk = chunk->next;
        }
        return false;
    }

    void freeAllChunks()
    {
        if constexpr (std::is_same_v<AllocPolicy, HeapAllocationPolicy>)
        {
            auto* chunk = chunks_;
            while (chunk)
            {
                auto* next = chunk->next;
                
                // Destroy all nodes in this chunk
                char* ptr = chunk->pool;
                for (size_t i = 0; i < chunk->size; ++i)
                {
                    Node* node = reinterpret_cast<Node*>(ptr);
                    node->data.~T();
                    ptr += sizeof(Node);
                }
                
                AllocPolicy::template deallocateChunk<Node>(chunk);
                chunk = next;
            }
            chunks_ = nullptr;
        }
    }

    size_t poolSize_;
    size_t totalCapacity_;
    Node* freeList_;
    
    using Chunk = typename HeapAllocationPolicy::Chunk;
    Chunk* chunks_;
};

} // namespace casket