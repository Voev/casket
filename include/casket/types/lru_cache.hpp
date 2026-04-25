#pragma once
#include <array>
#include <cstddef>
#include <cassert>
#include <casket/types/object_pool.hpp>

namespace casket
{

/// @brief LRU (least recently used) cache.
///
/// @tparam Key Type of cache key.
/// @tparam Value Type of cached value.
/// @tparam HASH_TABLE_SIZE Hash table buckets count.
///
template <typename Key, typename Value, size_t HASH_TABLE_SIZE = 16384>
class LruCache final
{
private:
    struct Node
    {
        Value value;
        Key key;
        Node* lruPrev{nullptr};
        Node* lruNext{nullptr};
        Node* hashNext{nullptr};

        template <typename... Args>
        Node(Args&&... args)
            : value(std::forward<Args>(args)...)
            , key()
            , lruPrev(nullptr)
            , lruNext(nullptr)
            , hashNext(nullptr)
        {
        }

        Node() = default;
    };

    using PoolType = ObjectPool<Node, StrictHeapPolicy>;

public:
    template <typename... Args>
    explicit LruCache(const size_t poolSize, Args&&... args)
        : pool_(poolSize, std::forward<Args>(args)...)
        , lruHead_(nullptr)
        , lruTail_(nullptr)
        , size_(0)
    {
        hashTable_.fill(nullptr);
    }

    ~LruCache() = default;

    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    template <typename... Args>
    Value* put(const Key& key, Args&&... args)
    {
        remove(key);

        auto* node = pool_.acquire(std::forward<Args>(args)...);
        if (!node)
        {
            if (size_ > 0)
            {
                evictLRU();
                node = pool_.acquire(std::forward<Args>(args)...);
            }
            if (!node)
            {
                return nullptr;
            }
        }

        node->key = key;
        node->lruPrev = nullptr;
        node->lruNext = nullptr;
        node->hashNext = nullptr;

        insertIntoHashTable(node);
        addToLRU(node);

        size_++;

        return &node->value;
    }

    Value* get(const Key& key)
    {
        Node* node = find(key);
        if (node)
        {
            touch(node);
            return &node->value;
        }
        return nullptr;
    }

    void release(const Key& key)
    {
        Node* node = find(key);
        if (node)
        {
            touch(node);
        }
    }

    bool remove(const Key& key)
    {
        Node* node = find(key);
        if (!node)
        {
            return false;
        }

        removeFromCache(node);
        return true;
    }

    size_t size() const
    {
        return size_;
    }

    bool contains(const Key& key) const
    {
        return find(key) != nullptr;
    }

    void clear()
    {
        for (size_t i = 0; i < hashTable_.size(); ++i)
        {
            Node* node = hashTable_[i];
            while (node)
            {
                Node* next = node->hashNext;

                pool_.release(node);

                node = next;
            }
        }

        hashTable_.fill(nullptr);
        lruHead_ = nullptr;
        lruTail_ = nullptr;
        size_ = 0;
    }

private:

    size_t hash(const Key& key) const
    {
        return std::hash<Key>{}(key) & (HASH_TABLE_SIZE - 1);
    }

    Node* find(const Key& key) const
    {
        size_t h = hash(key);
        Node* node = hashTable_[h];

        while (node)
        {
            if (node->key == key)
            {
                return node;
            }
            node = node->hashNext;
        }
        return nullptr;
    }

    void insertIntoHashTable(Node* node)
    {
        size_t h = hash(node->key);
        node->hashNext = hashTable_[h];
        hashTable_[h] = node;
    }

    void removeFromHashTable(Node* node)
    {
        size_t h = hash(node->key);
        Node* prev = nullptr;
        Node* current = hashTable_[h];

        while (current)
        {
            if (current == node)
            {
                if (prev)
                {
                    prev->hashNext = current->hashNext;
                }
                else
                {
                    hashTable_[h] = current->hashNext;
                }
                break;
            }
            prev = current;
            current = current->hashNext;
        }
    }

    void addToLRU(Node* node)
    {
        node->lruPrev = nullptr;
        node->lruNext = lruHead_;

        if (lruHead_)
        {
            lruHead_->lruPrev = node;
        }

        lruHead_ = node;

        if (!lruTail_)
        {
            lruTail_ = node;
        }
    }

    void removeFromLRU(Node* node)
    {
        if (node->lruPrev)
        {
            node->lruPrev->lruNext = node->lruNext;
        }
        else
        {
            lruHead_ = node->lruNext;
        }

        if (node->lruNext)
        {
            node->lruNext->lruPrev = node->lruPrev;
        }
        else
        {
            lruTail_ = node->lruPrev;
        }
    }

    void touch(Node* node)
    {
        removeFromLRU(node);
        addToLRU(node);
    }

    void evictLRU()
    {
        if (lruTail_)
        {
            removeFromCache(lruTail_);
        }
    }

    void removeFromCache(Node* node)
    {
        removeFromHashTable(node);
        removeFromLRU(node);

        pool_.release(node);
        size_--;
    }

private:
    PoolType pool_;
    std::array<Node*, HASH_TABLE_SIZE> hashTable_;
    Node* lruHead_;
    Node* lruTail_;
    size_t size_;
};

} // namespace casket