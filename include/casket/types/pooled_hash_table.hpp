#pragma once
#include <array>
#include <cstddef>
#include <cassert>
#include <casket/types/object_pool.hpp>

namespace casket
{

template <typename Key, typename Value, size_t HASH_TABLE_SIZE = 16384, typename AllocPolicy = StrictHeapPolicy>
class PooledHashTable final
{
private:
    struct HashNode
    {
        Value value;
        Key key;
        HashNode* hashNext{nullptr};

        template <typename... Args>
        HashNode(Args&&... args)
            : value(std::forward<Args>(args)...)
            , key()
            , hashNext(nullptr)
        {
        }

        HashNode() = default;
    };

    using PoolType = ObjectPool<HashNode, AllocPolicy>;

public:
    template <typename... Args>
    explicit PooledHashTable(size_t poolSize, Args&&... args)
        : pool_(poolSize, std::forward<Args>(args)...)
        , size_(0)
    {
        hashTable_.fill(nullptr);
    }

    ~PooledHashTable() noexcept
    {
        clear();
    }

    PooledHashTable(const PooledHashTable&) = delete;
    PooledHashTable& operator=(const PooledHashTable&) = delete;

    template <typename... Args>
    Value* put(const Key& key, Args&&... args)
    {
        remove(key);

        auto* node = pool_.acquire(std::forward<Args>(args)...);
        if (!node)
        {
            return nullptr;
        }

        node->key = key;
        node->hashNext = nullptr;

        insertIntoHashTable(node);
        size_++;

        return &node->value;
    }

    Value* get(const Key& key)
    {
        HashNode* node = find(key);
        if (node)
        {
            return &node->value;
        }
        return nullptr;
    }

    bool remove(const Key& key)
    {
        HashNode* node = find(key);
        if (!node)
        {
            return false;
        }

        removeFromHashTable(node);
        pool_.release(node);

        size_--;
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
            HashNode* node = hashTable_[i];
            while (node)
            {
                HashNode* next = node->hashNext;
                pool_.release(node);
                node = next;
            }
        }

        hashTable_.fill(nullptr);
        size_ = 0;
    }

private:
    size_t hash(const Key& key) const
    {
        return std::hash<Key>{}(key) & (HASH_TABLE_SIZE - 1);
    }

    HashNode* find(const Key& key) const
    {
        size_t h = hash(key);
        HashNode* node = hashTable_[h];

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

    void insertIntoHashTable(HashNode* node)
    {
        size_t h = hash(node->key);
        node->hashNext = hashTable_[h];
        hashTable_[h] = node;
    }

    void removeFromHashTable(HashNode* node)
    {
        size_t h = hash(node->key);
        HashNode* prev = nullptr;
        HashNode* current = hashTable_[h];

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

private:
    PoolType pool_;
    std::array<HashNode*, HASH_TABLE_SIZE> hashTable_;
    size_t size_;
};

} // namespace casket