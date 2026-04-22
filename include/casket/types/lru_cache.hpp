#pragma once
#include <array>
#include <cstddef>
#include <cassert>
#include <casket/types/node_pool.hpp>

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
    struct CachedValue
    {
        Value value;
        Key key;
        CachedValue* lruPrev{nullptr};
        CachedValue* lruNext{nullptr};
        CachedValue* hashNext{nullptr};

        template <typename... Args>
        CachedValue(Args&&... args)
            : value(std::forward<Args>(args)...)
            , key()
            , lruPrev(nullptr)
            , lruNext(nullptr)
            , hashNext(nullptr)
        {
        }

        CachedValue() = default;
    };

    using PoolType = NodePool<CachedValue, StrictHeapPolicy>;

public:
    explicit LruCache(const size_t poolSize)
        : cachePool_(poolSize)
        , lruHead_(nullptr)
        , lruTail_(nullptr)
        , cacheSize_(0)
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

        auto* poolNode = cachePool_.acquire();
        if (!poolNode)
        {
            if (cacheSize_ > 0)
            {
                evictLRU();
                poolNode = cachePool_.acquire();
            }
            if (!poolNode)
            {
                return nullptr;
            }
        }

        new (&poolNode->data.value) Value(std::forward<Args>(args)...);
        CachedValue* node = &poolNode->data;

        node->key = key;
        node->lruPrev = nullptr;
        node->lruNext = nullptr;
        node->hashNext = nullptr;

        insertIntoHashTable(node);
        addToLRU(node);

        cacheSize_++;

        return &node->value;
    }

    Value* get(const Key& key)
    {
        CachedValue* node = find(key);
        if (node)
        {
            touch(node);
            return &node->value;
        }
        return nullptr;
    }

    void release(const Key& key)
    {
        CachedValue* node = find(key);
        if (node)
        {
            touch(node);
        }
    }

    bool remove(const Key& key)
    {
        CachedValue* node = find(key);
        if (!node)
        {
            return false;
        }

        removeFromCache(node);
        return true;
    }

    size_t size() const
    {
        return cacheSize_;
    }

    bool contains(const Key& key) const
    {
        return find(key) != nullptr;
    }

    void clear()
    {
        for (size_t i = 0; i < hashTable_.size(); ++i)
        {
            CachedValue* node = hashTable_[i];
            while (node)
            {
                CachedValue* next = node->hashNext;

                node->value.~Value();

                auto* poolNode = reinterpret_cast<typename PoolType::Node*>(reinterpret_cast<char*>(node) -
                                                                            offsetof(typename PoolType::Node, data));
                cachePool_.release(poolNode);

                node = next;
            }
        }

        hashTable_.fill(nullptr);
        lruHead_ = nullptr;
        lruTail_ = nullptr;
        cacheSize_ = 0;
    }

private:

    size_t hash(const Key& key) const
    {
        return std::hash<Key>{}(key) & (HASH_TABLE_SIZE - 1);
    }

    CachedValue* find(const Key& key) const
    {
        size_t h = hash(key);
        CachedValue* node = hashTable_[h];

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

    void insertIntoHashTable(CachedValue* node)
    {
        size_t h = hash(node->key);
        node->hashNext = hashTable_[h];
        hashTable_[h] = node;
    }

    void removeFromHashTable(CachedValue* node)
    {
        size_t h = hash(node->key);
        CachedValue* prev = nullptr;
        CachedValue* current = hashTable_[h];

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

    void addToLRU(CachedValue* node)
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

    void removeFromLRU(CachedValue* node)
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

    void touch(CachedValue* node)
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

    void removeFromCache(CachedValue* node)
    {
        removeFromHashTable(node);
        removeFromLRU(node);

        node->value.~Value();

        auto* poolNode = reinterpret_cast<typename PoolType::Node*>(reinterpret_cast<char*>(node) -
                                                                    offsetof(typename PoolType::Node, data));
        cachePool_.release(poolNode);

        cacheSize_--;
    }

private:
    PoolType cachePool_;
    std::array<CachedValue*, HASH_TABLE_SIZE> hashTable_;
    CachedValue* lruHead_;
    CachedValue* lruTail_;
    size_t cacheSize_;
};

} // namespace casket