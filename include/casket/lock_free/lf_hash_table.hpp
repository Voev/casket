#pragma once
#include <array>
#include <cstddef>
#include <cassert>
#include <atomic>
#include <thread>
#include <casket/lock_free/lf_object_pool.hpp>

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
        std::atomic<bool> deleted{false};

        HashNode() = default;

        template <typename K, typename... Args>
        HashNode(K&& key, Args&&... args)
            : key(std::forward<K>(key))
            , value(std::forward<Args>(args)...)
            , hashNext(nullptr)
            , deleted(false)
        {
        }

        HashNode(HashNode&& other) noexcept
            : key(std::move(other.key))
            , value(std::move(other.value))
            , hashNext(other.hashNext.load())
            , deleted(other.deleted.load())
        {
        }

        HashNode(const HashNode&) = delete;
        HashNode& operator=(const HashNode&) = delete;
    };

    struct Bucket
    {
        std::atomic<HashNode*> head{nullptr};

        HashNode* getHeadSafe() const
        {
            return head.load(std::memory_order_acquire);
        }

        void setHeadSafe(HashNode* node)
        {
            head.store(node, std::memory_order_release);
        }

        bool casHead(HashNode*& expected, HashNode* desired)
        {
            return head.compare_exchange_strong(expected, desired, std::memory_order_release,
                                                std::memory_order_acquire);
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
            if (!node->deleted.load(std::memory_order_acquire) && node->key == key)
            {
                return &node->value;
            }
            node = node->hashNext.load(std::memory_order_acquire);
        }

        return nullptr;
    }

    Value* put(const Key& key, Value&& value)
    {
        if (get(key) != nullptr)
        {
            return nullptr;
        }

        auto* node = pool_.acquire();
        if (!node)
        {
            return nullptr;
        }

        new (node) HashNode(key, std::move(value));
        node->deleted.store(false, std::memory_order_relaxed);

        size_t h = hash(key);
        Bucket& bucket = buckets_[h];

        while (true)
        {
            HashNode* oldHead = bucket.getHeadSafe();
            node->hashNext.store(oldHead, std::memory_order_relaxed);

            if (bucket.casHead(oldHead, node))
            {
                size_.fetch_add(1, std::memory_order_release);
                return &node->value;
            }

            if (get(key) != nullptr)
            {
                pool_.release(node);
                return nullptr;
            }

            std::this_thread::yield();
        }
    }

    Value* put(const Key& key, const Value& value)
    {
        return put(key, Value(value));
    }

    bool remove(const Key& key)
    {
        size_t h = hash(key);
        Bucket& bucket = buckets_[h];

        HashNode* prev = nullptr;
        HashNode* current = bucket.getHeadSafe();
        
        while (current)
        {
            if (!current->deleted.load(std::memory_order_acquire) && current->key == key)
            {
                bool expected = false;
                if (current->deleted.compare_exchange_strong(expected, true,
                                                              std::memory_order_release,
                                                              std::memory_order_acquire))
                {
                    HashNode* next = current->hashNext.load(std::memory_order_acquire);
                    
                    if (prev)
                    {
                        // Remove middle
                        prev->hashNext.store(next, std::memory_order_release);
                    }
                    else
                    {
                        // Remove head - need CAS
                        if (!bucket.casHead(current, next))
                        {
                            size_.fetch_sub(1, std::memory_order_release);
                            pool_.release(current);
                            return true;
                        }
                    }
                    
                    size_.fetch_sub(1, std::memory_order_release);
                    pool_.release(current);
                    return true;
                }
                return false;
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
                if (!node->deleted.load(std::memory_order_acquire))
                {
                    func(node->key, node->value);
                }
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
                if (!node->deleted.load(std::memory_order_acquire))
                {
                    func(node->key, node->value);
                }
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