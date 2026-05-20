#pragma once
#include <array>
#include <cstddef>
#include <cassert>
#include <atomic>
#include <thread>
#include <casket/lock_free/lf_object_pool.hpp>
#include <casket/concurrency/sequence_lock.hpp>

namespace casket::lf
{

template <typename Key, typename Value, size_t HASH_TABLE_SIZE = 16384>
class HashTable final
{
private:
    struct HashNode
    {
        Key key;
        Value value;
        std::atomic<HashNode*> hashNext{nullptr};

        // Default constructor
        HashNode() = default;

        // Constructor for creating node with key and value arguments
        template <typename K, typename... Args>
        HashNode(K&& key, Args&&... args)
            : key(std::forward<K>(key))
            , value(std::forward<Args>(args)...)
            , hashNext(nullptr)
        {
        }

        // Move constructor
        HashNode(HashNode&& other) noexcept
            : key(std::move(other.key))
            , value(std::move(other.value))
            , hashNext(other.hashNext.load())
        {
        }

        // Disable copy
        HashNode(const HashNode&) = delete;
        HashNode& operator=(const HashNode&) = delete;
    };

    struct Bucket
    {
        SequenceLock<HashNode*> headLock;

        Bucket()
        {
            headLock.store(nullptr);
        }

        HashNode* getHeadSafe()
        {
            HashNode* h = nullptr;
            headLock.load(h);
            return h;
        }

        void setHeadSafe(HashNode* node)
        {
            headLock.store(node);
        }
    };

public:
    template <typename... Args>
    explicit HashTable(size_t poolSize, Args&&... args)
        : pool_(poolSize, std::forward<Args>(args)...)
        , buckets_{}
        , size_{0}
    {
    }

    ~HashTable() noexcept
    {
        clear();
    }

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    Value* get(const Key& key)
    {
        size_t h = hash(key);
        Bucket& bucket = buckets_[h];

        HashNode* node = bucket.getHeadSafe();

        while (node)
        {
            if (node->key == key)
            {
                return &node->value;
            }
            node = node->hashNext.load(std::memory_order_acquire);
        }

        return nullptr;
    }

    Value* put(const Key& key, Value&& value)
    {
        Value* existing = get(key);
        if (existing)
        {
            return nullptr;
        }

        auto* node = pool_.acquire();
        if (!node)
        {
            return nullptr;
        }

        node->value = std::move(value);
        node->key = key;
        node->hashNext.store(nullptr, std::memory_order_relaxed);

        size_t h = hash(key);
        Bucket& bucket = buckets_[h];

        HashNode* oldHead = bucket.getHeadSafe();
        node->hashNext.store(oldHead, std::memory_order_relaxed);
        bucket.setHeadSafe(node);

        size_.fetch_add(1, std::memory_order_relaxed);
        return &node->value;
    }

    Value* put(const Key& key, const Value& value)
    {
        Value* existing = get(key);
        if (existing)
        {
            return nullptr;
        }

        auto* node = pool_.acquire();
        if (!node)
        {
            return nullptr;
        }

        node->value = value;
        node->key = key;
        node->hashNext.store(nullptr, std::memory_order_relaxed);

        size_t h = hash(key);
        Bucket& bucket = buckets_[h];

        HashNode* oldHead = bucket.getHeadSafe();
        node->hashNext.store(oldHead, std::memory_order_relaxed);
        bucket.setHeadSafe(node);

        size_.fetch_add(1, std::memory_order_relaxed);
        return &node->value;
    }

    bool remove(const Key& key)
    {
        size_t h = hash(key);
        Bucket& bucket = buckets_[h];

        HashNode* head = bucket.getHeadSafe();

        if (!head)
        {
            return false;
        }

        if (head->key == key)
        {
            HashNode* newHead = head->hashNext.load(std::memory_order_acquire);
            bucket.setHeadSafe(newHead);
            pool_.release(head);
            size_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }

        HashNode* prev = head;
        HashNode* current = head->hashNext.load(std::memory_order_acquire);

        while (current)
        {
            if (current->key == key)
            {
                HashNode* next = current->hashNext.load(std::memory_order_acquire);
                prev->hashNext.store(next, std::memory_order_release);
                pool_.release(current);
                size_.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
            prev = current;
            current = current->hashNext.load(std::memory_order_acquire);
        }

        return false;
    }

    bool contains(const Key& key) const
    {
        return const_cast<HashTable*>(this)->get(key) != nullptr;
    }

    size_t size() const
    {
        return size_.load(std::memory_order_relaxed);
    }

    void clear()
    {
        for (size_t i = 0; i < buckets_.size(); ++i)
        {
            HashNode* node = buckets_[i].getHeadSafe();
            buckets_[i].setHeadSafe(nullptr);

            while (node)
            {
                HashNode* next = node->hashNext.load(std::memory_order_acquire);
                pool_.release(node);
                node = next;
            }
        }
        size_.store(0, std::memory_order_relaxed);
    }

    template <typename Func>
    void forEach(Func&& func)
    {
        for (auto& bucket : buckets_)
        {
            HashNode* node = bucket.getHeadSafe();
            while (node)
            {
                func(node->key, node->value);
                node = node->hashNext.load(std::memory_order_acquire);
            }
        }
    }

    template <typename Func>
    void forEachCopy(Func&& func) const
    {
        for (const auto& bucket : buckets_)
        {
            HashNode* node = bucket.getHeadSafe();
            while (node)
            {
                func(node->key, node->value);
                node = node->hashNext.load(std::memory_order_acquire);
            }
        }
    }

private:
    size_t hash(const Key& key) const
    {
        return std::hash<Key>{}(key) & (HASH_TABLE_SIZE - 1);
    }

private:
    ObjectPool<HashNode> pool_;
    std::array<Bucket, HASH_TABLE_SIZE> buckets_;
    std::atomic<size_t> size_;
};

} // namespace casket::lf