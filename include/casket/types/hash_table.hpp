#pragma once
#include <array>
#include <cstddef>
#include <cassert>

namespace casket
{

template <typename Key, typename T, size_t HASH_TABLE_SIZE = 16384>
class HashTable final
{
private:
    struct HashNode
    {
        T* value{nullptr};
        Key key;
        HashNode* hashNext{nullptr};
        
        HashNode() = default;
        
        HashNode(const Key& k, T* v)
            : value(v)
            , key(k)
            , hashNext(nullptr)
        {
        }
    };

public:
    explicit HashTable(const size_t poolSize)
        : size_(0)
    {
        hashTable_.fill(nullptr);

        for (size_t i = 0; i < poolSize; ++i)
        {
            auto* node = new HashNode();
            node->hashNext = freeList_;
            freeList_ = node;
        }
    }

    ~HashTable()
    {
        clear();
        
        while (freeList_)
        {
            HashNode* next = freeList_->hashNext;
            delete freeList_;
            freeList_ = next;
        }
    }

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    T* insert(const Key& key, T* value)
    {
        remove(key);
        
        HashNode* node = allocateNode();
        if (!node)
        {
            return nullptr;
        }
        
        node->key = key;
        node->value = value;
        node->hashNext = nullptr;
        
        insertIntoHashTable(node);
        size_++;
        
        return value;
    }

    T* get(const Key& key) const
    {
        HashNode* node = find(key);
        return node ? node->value : nullptr;
    }

    bool remove(const Key& key)
    {
        HashNode* node = find(key);
        if (!node)
        {
            return false;
        }
        
        removeFromHashTable(node);
        
        node->value = nullptr;
        node->key = Key();
        
        releaseNode(node);
        
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
                node->value = nullptr;
                releaseNode(node);
                node = next;
            }
            hashTable_[i] = nullptr;
        }
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
    
    HashNode* allocateNode()
    {
        if (freeList_)
        {
            HashNode* node = freeList_;
            freeList_ = freeList_->hashNext;
            return node;
        }
        return new HashNode();
    }
    
    void releaseNode(HashNode* node)
    {
        if (node)
        {
            node->hashNext = freeList_;
            freeList_ = node;
        }
    }

private:
    std::array<HashNode*, HASH_TABLE_SIZE> hashTable_;
    HashNode* freeList_{nullptr};
    size_t size_;
};

} // namespace casket