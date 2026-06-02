#pragma once
#include <chrono>
#include <vector>
#include <casket/types/flat_hash_table.hpp>

namespace casket
{

/// @brief TTL cache with LRU eviction policy and O(1) time complexity.
/// @tparam Key cache key type
/// @tparam Value cache value type
template <typename Key, typename Value>
class TtlCache
{
private:
    struct Node
    {
        Key key;
        Value value;
        std::chrono::steady_clock::time_point expiry;
        int prev;
        int next;

        Node()
            : prev(-1)
            , next(-1)
        {
        }
        Node(Key&& k, Value&& v, std::chrono::steady_clock::time_point exp)
            : key(std::move(k))
            , value(std::move(v))
            , expiry(exp)
            , prev(-1)
            , next(-1)
        {
        }
    };

    std::vector<Node> pool_;
    mutable RobinHoodHashMap<Key, int> keyToIndex_;

    int head_ = -1;
    int tail_ = -1;
    int freeList_ = -1;
    size_t maxSize_;
    size_t size_ = 0;

    void initFreeList()
    {
        for (int i = 0; i < static_cast<int>(maxSize_) - 1; ++i)
        {
            pool_[i].next = i + 1;
        }
        if (maxSize_ > 0)
        {
            pool_[maxSize_ - 1].next = -1;
            freeList_ = 0;
        }
    }

    void moveToFront(int index)
    {
        if (index == head_)
            return;

        if (pool_[index].prev != -1)
            pool_[pool_[index].prev].next = pool_[index].next;
        if (pool_[index].next != -1)
            pool_[pool_[index].next].prev = pool_[index].prev;
        if (index == tail_)
            tail_ = pool_[index].prev;

        pool_[index].prev = -1;
        pool_[index].next = head_;
        if (head_ != -1)
            pool_[head_].prev = index;
        head_ = index;

        if (tail_ == -1)
            tail_ = head_;
    }

    int allocateNode()
    {
        if (freeList_ == -1)
            return -1;
        int index = freeList_;
        freeList_ = pool_[freeList_].next;
        pool_[index].next = -1;
        pool_[index].prev = -1;
        return index;
    }

    void freeNode(int index)
    {
        pool_[index].next = freeList_;
        freeList_ = index;
    }

public:
    /// @brief Constructs a TTL cache with fixed maximum size.
    /// @param[in] maxSize maximum number of elements in the cache.
    /// @throws std::invalid_argument if maxSize is zero.
    explicit TtlCache(size_t maxSize)
        : keyToIndex_(maxSize * 2)
        , maxSize_(maxSize)
    {
        if (maxSize == 0)
        {
            throw std::invalid_argument("TtlCache: maxSize must be > 0");
        }

        pool_.reserve(maxSize);
        pool_.resize(maxSize);

        initFreeList();
        keyToIndex_.reserve(maxSize * 2);
    }

    /// @brief Copy constructor is deleted.
    TtlCache(const TtlCache&) = delete;

    /// @brief Copy assignment operator is deleted.
    TtlCache& operator=(const TtlCache&) = delete;

    /// @brief Move constructor.
    /// @param[in] other source cache to move from.
    TtlCache(TtlCache&& other) noexcept
        : pool_(std::move(other.pool_))
        , keyToIndex_(std::move(other.keyToIndex_))
        , head_(other.head_)
        , tail_(other.tail_)
        , freeList_(other.freeList_)
        , maxSize_(other.maxSize_)
        , size_(other.size_)
    {
        other.head_ = other.tail_ = other.freeList_ = -1;
        other.size_ = 0;
    }

    /// @brief Move assignment operator.
    /// @param[in] other source cache to move from.
    /// @return reference to this cache.
    TtlCache& operator=(TtlCache&& other) noexcept
    {
        if (this != &other)
        {
            pool_ = std::move(other.pool_);
            keyToIndex_ = std::move(other.keyToIndex_);
            head_ = other.head_;
            tail_ = other.tail_;
            freeList_ = other.freeList_;
            maxSize_ = other.maxSize_;
            size_ = other.size_;

            other.head_ = other.tail_ = other.freeList_ = -1;
            other.size_ = 0;
        }
        return *this;
    }

    /// @brief Retrieves a value by key and updates its LRU position.
    /// @param[in] key key to look up.
    /// @param[in] now time point to expiry.
    ///
    /// @return pointer to the value, or nullptr if key not found or expired.
    /// @note The returned pointer is valid until the element is erased or the cache is modified.
    Value* get(const Key& key, const std::chrono::steady_clock::time_point& now = std::chrono::steady_clock::now())
    {
        int* indexPtr = keyToIndex_.find(key);
        if (!indexPtr)
            return nullptr;

        int index = *indexPtr;
        if (pool_[index].expiry <= now)
        {
            erase(key);
            return nullptr;
        }

        moveToFront(index);
        return &pool_[index].value;
    }

    /// @brief Inserts or updates a value with a specified expiry time.
    /// @param[in] key insertion key.
    /// @param[in] value rvalue reference to the value to insert.
    /// @param[in] expiry time point when the entry expires.
    void put(const Key& key, Value&& value, std::chrono::steady_clock::time_point expiry)
    {
        int* existingIndex = keyToIndex_.find(key);
        if (existingIndex)
        {
            int index = *existingIndex;
            pool_[index].value = std::move(value);
            pool_[index].expiry = expiry;
            moveToFront(index);
            return;
        }

        if (size_ >= maxSize_)
        {
            if (tail_ != -1)
            {
                keyToIndex_.erase(pool_[tail_].key);
                int oldTail = tail_;
                tail_ = pool_[tail_].prev;
                if (tail_ != -1)
                    pool_[tail_].next = -1;
                freeNode(oldTail);
                size_--;
            }
        }

        int newIndex = allocateNode();
        if (newIndex != -1)
        {
            pool_[newIndex].key = key;
            pool_[newIndex].value = std::move(value);
            pool_[newIndex].expiry = expiry;
            keyToIndex_.insert(key, newIndex);

            pool_[newIndex].prev = -1;
            pool_[newIndex].next = head_;
            if (head_ != -1)
                pool_[head_].prev = newIndex;
            head_ = newIndex;

            if (tail_ == -1)
                tail_ = head_;

            size_++;
        }
    }

    /// @brief Inserts or updates a value with a specified expiry time.
    /// @param[in] key insertion key.
    /// @param[in] value const lvalue reference to the value to insert.
    /// @param[in] expiry time point when the entry expires.
    void put(const Key& key, const Value& value, std::chrono::steady_clock::time_point expiry)
    {
        Value copy = value;
        put(key, std::move(copy), expiry);
    }

    /// @brief Removes all elements from the cache.
    void clear()
    {
        keyToIndex_.clear();
        head_ = tail_ = -1;
        size_ = 0;
        initFreeList();
    }

    /// @brief Returns the current number of elements in the cache.
    /// @return number of stored elements.
    size_t size() const
    {
        return size_;
    }

    /// @brief Returns the maximum capacity of the cache.
    /// @return maximum number of elements.
    size_t capacity() const
    {
        return maxSize_;
    }

    /// @brief Checks if the cache is empty.
    /// @return true if empty, false otherwise.
    bool empty() const
    {
        return size_ == 0;
    }

    /// @brief Checks if the cache is full.
    /// @return true if size equals capacity, false otherwise.
    bool full() const
    {
        return size_ >= maxSize_;
    }

    /// @brief Checks if a key exists and is not expired.
    /// @param[in] key key to check.
    /// @return true if key exists and not expired, false otherwise.
    bool contains(const Key& key) const
    {
        auto now = std::chrono::steady_clock::now();
        int* indexPtr = keyToIndex_.find(key);
        if (!indexPtr)
            return false;
        return pool_[*indexPtr].expiry > now;
    }

    /// @brief Removes a specific key from the cache.
    /// @param[in] key key to remove.
    void erase(const Key& key)
    {
        int* indexPtr = keyToIndex_.find(key);
        if (!indexPtr)
            return;

        int index = *indexPtr;

        if (pool_[index].prev != -1)
            pool_[pool_[index].prev].next = pool_[index].next;
        if (pool_[index].next != -1)
            pool_[pool_[index].next].prev = pool_[index].prev;
        if (index == head_)
            head_ = pool_[index].next;
        if (index == tail_)
            tail_ = pool_[index].prev;

        keyToIndex_.erase(key);
        freeNode(index);
        size_--;
    }

    /// @brief Resizes the cache, clearing all existing data.
    /// @param[in] newMaxSize new maximum capacity.
    /// @throws std::invalid_argument if newMaxSize is zero.
    void resize(size_t newMaxSize)
    {
        if (newMaxSize == 0)
        {
            throw std::invalid_argument("TtlCache: newMaxSize must be > 0");
        }

        clear();
        maxSize_ = newMaxSize;
        pool_.clear();
        pool_.resize(newMaxSize);
        keyToIndex_.clear();
        keyToIndex_.reserve(newMaxSize * 2);
        initFreeList();
    }
};

} // namespace casket