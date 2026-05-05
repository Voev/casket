#pragma once

#include <memory>
#include <functional>
#include <type_traits>
#include <casket/nonstd/optional.hpp>

namespace casket
{

struct LinearProbing
{
    static constexpr const char* name = "Linear";
    static constexpr float defaultMaxLoadFactor = 0.75f;

    template <typename Impl>
    static size_t next(Impl& impl, size_t current, uint64_t hash, uint32_t attempt)
    {
        (void)hash;
        (void)attempt;
        return (current + 1) & impl.mask;
    }

    static bool shouldSwap(uint32_t currentDistance, uint32_t entryDistance)
    {
        (void)currentDistance;
        (void)entryDistance;
        return false;
    }
};

struct RobinHoodProbing
{
    static constexpr const char* name = "RobinHood";
    static constexpr float defaultMaxLoadFactor = 0.9f;

    template <typename Impl>
    static size_t next(Impl& impl, size_t current, uint64_t hash, uint32_t attempt)
    {
        (void)hash;
        (void)attempt;
        return (current + 1) & impl.mask;
    }

    static bool shouldSwap(uint32_t currentDistance, uint32_t entryDistance)
    {
        return currentDistance > entryDistance;
    }
};

struct QuadraticProbing
{
    static constexpr const char* name = "Quadratic";
    static constexpr float defaultMaxLoadFactor = 0.75f;

    template <typename Impl>
    static size_t next(Impl& impl, size_t current, uint64_t hash, uint32_t attempt)
    {
        (void)hash;
        return (current + attempt * attempt) & impl.mask;
    }

    static bool shouldSwap(uint32_t currentDistance, uint32_t entryDistance)
    {
        (void)currentDistance;
        (void)entryDistance;
        return false;
    }
};

template <typename SecondHash = std::hash<uint64_t>>
struct DoubleHashingProbing
{
    static constexpr const char* name = "DoubleHashing";
    static constexpr float defaultMaxLoadFactor = 0.85f;

    template <typename Impl>
    static size_t next(Impl& impl, size_t current, uint64_t hash, uint32_t attempt)
    {
        (void)impl;
        if (attempt == 0)
            return current;
        uint64_t step = (hash >> 32) | 1;
        return (current + step) & impl.mask;
    }

    static bool shouldSwap(uint32_t currentDistance, uint32_t entryDistance)
    {
        (void)currentDistance;
        (void)entryDistance;
        return false;
    }
};

template <typename Key, typename Value>
struct FlatEntry
{
    Key key;
    Value value;
    uint64_t hash;
    uint32_t distance;
    bool valid;

    FlatEntry(Key&& k, Value&& v, uint64_t h, uint32_t d)
        : key(std::move(k))
        , value(std::move(v))
        , hash(h)
        , distance(d)
        , valid(true)
    {
    }

    FlatEntry(Key&& k, Value&& v, uint64_t h)
        : key(std::move(k))
        , value(std::move(v))
        , hash(h)
        , distance(0)
        , valid(true)
    {
    }

    FlatEntry()
        : key()
        , value()
        , hash(0)
        , distance(0)
        , valid(false)
    {
    }

    FlatEntry(FlatEntry&&) = default;
    FlatEntry& operator=(FlatEntry&&) = default;

    FlatEntry(const FlatEntry&) = delete;
    FlatEntry& operator=(const FlatEntry&) = delete;

    void invalidate()
    {
        valid = false;
    }
};

template <typename Key, typename Value, typename ProbingStrategy = LinearProbing, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class FlatHashMap
{
public:
    using Entry = FlatEntry<Key, Value>;

    class Iterator;
    class ConstIterator;

private:
    struct Impl
    {
        std::unique_ptr<Entry[], decltype(&std::free)> slots{nullptr, std::free};
        size_t capacity;
        size_t mask;
        size_t size{0};
        uint32_t maxDistance{0};

        Impl(size_t cap)
            : capacity(cap)
            , mask(cap - 1)
        {
            void* raw = std::aligned_alloc(alignof(Entry), sizeof(Entry) * cap);
            if (!raw)
                throw std::bad_alloc();
            slots.reset(static_cast<Entry*>(raw));
            for (size_t i = 0; i < cap; ++i)
            {
                new (&slots[i]) Entry();
            }
        }

        ~Impl()
        {
            if (slots)
            {
                for (size_t i = 0; i < capacity; ++i)
                {
                    slots[i].~Entry();
                }
            }
        }

        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;
        Impl(Impl&&) = default;
        Impl& operator=(Impl&&) = default;

        size_t nextValidIndex(size_t start) const
        {
            for (size_t i = start; i < capacity; ++i)
            {
                if (slots[i].valid)
                    return i;
            }
            return capacity;
        }
    };

    std::unique_ptr<Impl> impl_;
    float maxLoadFactor_;
    Hash hasher_;
    KeyEqual equal_;

public:
    explicit FlatHashMap(size_t initialCapacity = 1024, float loadFactor = ProbingStrategy::defaultMaxLoadFactor)
        : impl_(std::make_unique<Impl>(nextPowerOfTwo(initialCapacity)))
        , maxLoadFactor_(loadFactor)
    {
    }

    FlatHashMap(FlatHashMap&&) = default;
    FlatHashMap& operator=(FlatHashMap&&) = default;

    FlatHashMap(const FlatHashMap&) = delete;
    FlatHashMap& operator=(const FlatHashMap&) = delete;

    Iterator begin()
    {
        size_t idx = impl_->nextValidIndex(0);
        return Iterator(this, idx);
    }

    ConstIterator begin() const
    {
        size_t idx = impl_->nextValidIndex(0);
        return ConstIterator(this, idx);
    }

    ConstIterator cbegin() const
    {
        return begin();
    }

    Iterator end()
    {
        return Iterator(this, impl_->capacity);
    }

    ConstIterator end() const
    {
        return ConstIterator(this, impl_->capacity);
    }

    ConstIterator cend() const
    {
        return end();
    }

    bool insert(const Key& key, Value&& value)
    {
        return insertImpl(key, std::move(value));
    }

    bool insert(Key&& key, Value&& value)
    {
        return insertImpl(std::move(key), std::move(value));
    }

    bool insert(const Key& key, const Value& value)
    {
        return insertImpl(key, value);
    }

    bool insert(Key&& key, const Value& value)
    {
        return insertImpl(std::move(key), value);
    }

    const Value* find(const Key& key) const
    {
        uint64_t hash = hasher_(key);
        size_t idx = hash & impl_->mask;
        uint32_t distance = 0;
        uint32_t attempt = 0;

        while (true)
        {
            const auto& entry = impl_->slots[idx];

            if (!entry.valid)
            {
                return nullptr;
            }

            if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
            {
                if (distance > entry.distance)
                {
                    return nullptr;
                }
            }

            if (entry.hash == hash && equal_(entry.key, key))
            {
                return &entry.value;
            }

            ++attempt;
            idx = ProbingStrategy::next(*impl_, idx, hash, attempt);
            ++distance;
        }
    }

    Value* find(const Key& key)
    {
        return const_cast<Value*>(const_cast<const FlatHashMap*>(this)->find(key));
    }

    bool contains(const Key& key) const
    {
        return find(key) != nullptr;
    }

    bool erase(const Key& key)
    {
        uint64_t hash = hasher_(key);
        size_t idx = hash & impl_->mask;
        uint32_t distance = 0;
        uint32_t attempt = 0;

        while (true)
        {
            auto& entry = impl_->slots[idx];

            if (!entry.valid)
            {
                return false;
            }

            if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
            {
                if (distance > entry.distance)
                {
                    return false;
                }
            }

            if (entry.hash == hash && equal_(entry.key, key))
            {
                entry.valid = false;
                --impl_->size;

                if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
                {
                    shiftBackwards(idx);
                }
                return true;
            }

            ++attempt;
            idx = ProbingStrategy::next(*impl_, idx, hash, attempt);
            ++distance;
        }
    }

    void reserve(size_t newCapacity)
    {
        if (newCapacity > impl_->capacity)
        {
            rehash(newCapacity);
        }
    }

    size_t size() const
    {
        return impl_->size;
    }

    size_t capacity() const
    {
        return impl_->capacity;
    }

    float loadFactor() const
    {
        return static_cast<float>(impl_->size) / impl_->capacity;
    }

    uint32_t maxProbeDistance() const
    {
        return impl_->maxDistance;
    }

    bool empty() const
    {
        return impl_->size == 0;
    }

    void clear()
    {
        for (size_t i = 0; i < impl_->capacity; ++i)
        {
            if (impl_->slots[i].valid)
            {
                impl_->slots[i].~Entry();
                new (&impl_->slots[i]) Entry();
            }
        }
        impl_->size = 0;
        impl_->maxDistance = 0;
    }

    class Iterator
    {
    private:
        const FlatHashMap* map_;
        size_t index_;

        void advanceToNextValid()
        {
            while (index_ < map_->impl_->capacity && !map_->impl_->slots[index_].valid)
            {
                ++index_;
            }
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = std::pair<const Key*, Value*>;
        using reference = std::pair<const Key&, Value&>;

        Iterator()
            : map_(nullptr)
            , index_(0)
        {
        }

        Iterator(const FlatHashMap* map, size_t index)
            : map_(map)
            , index_(index)
        {
            if (map_ && index_ < map_->impl_->capacity)
            {
                advanceToNextValid();
            }
        }

        reference operator*()
        {
            auto& entry = map_->impl_->slots[index_];
            return {entry.key, entry.value};
        }

        pointer operator->()
        {
            auto& entry = map_->impl_->slots[index_];
            return {&entry.key, &entry.value};
        }

        Iterator& operator++()
        {
            ++index_;
            advanceToNextValid();
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const
        {
            return map_ == other.map_ && index_ == other.index_;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }
    };

    class ConstIterator
    {
    private:
        const FlatHashMap* map_;
        size_t index_;

        void advanceToNextValid()
        {
            while (index_ < map_->impl_->capacity && !map_->impl_->slots[index_].valid)
            {
                ++index_;
            }
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, const Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = std::pair<const Key*, const Value*>;
        using reference = std::pair<const Key&, const Value&>;

        ConstIterator()
            : map_(nullptr)
            , index_(0)
        {
        }

        ConstIterator(const FlatHashMap* map, size_t index)
            : map_(map)
            , index_(index)
        {
            if (map_ && index_ < map_->impl_->capacity)
            {
                advanceToNextValid();
            }
        }

        ConstIterator(const Iterator& other)
            : map_(other.map_)
            , index_(other.index_)
        {
        }

        reference operator*() const
        {
            auto& entry = map_->impl_->slots[index_];
            return {entry.key, entry.value};
        }

        pointer operator->() const
        {
            auto& entry = map_->impl_->slots[index_];
            return {&entry.key, &entry.value};
        }

        ConstIterator& operator++()
        {
            ++index_;
            advanceToNextValid();
            return *this;
        }

        ConstIterator operator++(int)
        {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const ConstIterator& other) const
        {
            return map_ == other.map_ && index_ == other.index_;
        }

        bool operator!=(const ConstIterator& other) const
        {
            return !(*this == other);
        }
    };

private:
    template <typename K, typename V>
    bool insertImpl(K&& key, V&& value)
    {
        if (impl_->size >= impl_->capacity)
        {
            rehash(impl_->capacity * 2);
        }

        uint64_t hash = hasher_(std::forward<K>(key));
        size_t idx = hash & impl_->mask;
        uint32_t attempt = 0;

        using DecayedK = std::decay_t<K>;
        using DecayedV = std::decay_t<V>;

        DecayedK currentKey = std::forward<K>(key);
        DecayedV currentValue = std::forward<V>(value);
        uint64_t currentHash = hash;
        uint32_t currentDistance = 0;

        while (true)
        {
            auto& entry = impl_->slots[idx];

            if (!entry.valid)
            {
                if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
                {
                    new (&entry) Entry(std::move(currentKey), std::move(currentValue), currentHash, currentDistance);
                    if (currentDistance > impl_->maxDistance)
                    {
                        impl_->maxDistance = currentDistance;
                    }
                }
                else
                {
                    new (&entry) Entry(std::move(currentKey), std::move(currentValue), currentHash);
                }

                ++impl_->size;

                if (shouldRehash())
                {
                    rehash(impl_->capacity * 2);
                }
                return true;
            }

            if (entry.hash == currentHash && equal_(entry.key, currentKey))
            {
                entry.value = std::move(currentValue);
                return false;
            }

            if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
            {
                if (ProbingStrategy::shouldSwap(currentDistance, entry.distance))
                {
                    std::swap(currentKey, entry.key);
                    std::swap(currentValue, entry.value);
                    std::swap(currentHash, entry.hash);
                    std::swap(currentDistance, entry.distance);
                }
            }

            ++attempt;
            idx = ProbingStrategy::next(*impl_, idx, currentHash, attempt);
            ++currentDistance;

            if (currentDistance > impl_->capacity)
            {
                rehash(impl_->capacity * 2);
                currentDistance = 0;
                attempt = 0;
                idx = currentHash & impl_->mask;
            }
        }
    }

    template <typename K, typename V>
    void insertImplNoRehash(K&& key, V&& value)
    {
        uint64_t hash = hasher_(std::forward<K>(key));
        size_t idx = hash & impl_->mask;
        uint32_t attempt = 0;

        using DecayedK = std::decay_t<K>;
        using DecayedV = std::decay_t<V>;

        DecayedK currentKey = std::forward<K>(key);
        DecayedV currentValue = std::forward<V>(value);
        uint64_t currentHash = hash;
        uint32_t currentDistance = 0;

        while (true)
        {
            auto& entry = impl_->slots[idx];

            if (!entry.valid)
            {
                if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
                {
                    new (&entry) Entry(std::move(currentKey), std::move(currentValue), currentHash, currentDistance);
                    if (currentDistance > impl_->maxDistance)
                    {
                        impl_->maxDistance = currentDistance;
                    }
                }
                else
                {
                    new (&entry) Entry(std::move(currentKey), std::move(currentValue), currentHash);
                }

                ++impl_->size;
                return;
            }

            if (entry.hash == currentHash && equal_(entry.key, currentKey))
            {
                entry.value = std::move(currentValue);
                return;
            }

            if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
            {
                if (ProbingStrategy::shouldSwap(currentDistance, entry.distance))
                {
                    std::swap(currentKey, entry.key);
                    std::swap(currentValue, entry.value);
                    std::swap(currentHash, entry.hash);
                    std::swap(currentDistance, entry.distance);
                }
            }

            ++attempt;
            idx = ProbingStrategy::next(*impl_, idx, currentHash, attempt);
            ++currentDistance;
        }
    }

    bool shouldRehash() const
    {
        return impl_->size > static_cast<size_t>(impl_->capacity * maxLoadFactor_);
    }

    static size_t nextPowerOfTwo(size_t n)
    {
        size_t power = 1;
        while (power < n)
            power <<= 1;
        return power;
    }

    void shiftBackwards(size_t startIdx)
    {
        if constexpr (std::is_same_v<ProbingStrategy, RobinHoodProbing>)
        {
            size_t idx = (startIdx + 1) & impl_->mask;
            uint32_t distance = 1;

            while (true)
            {
                auto& entry = impl_->slots[idx];

                if (!entry.valid)
                    return;
                if (entry.distance < distance)
                    return;

                size_t prevIdx = (idx - 1) & impl_->mask;
                auto& prevEntry = impl_->slots[prevIdx];

                new (&prevEntry) Entry(std::move(entry));
                entry.invalidate();

                idx = (idx + 1) & impl_->mask;
                ++distance;
            }
        }
    }

    void rehash(size_t newCapacity)
    {
        auto oldImpl = std::move(impl_);
        impl_ = std::make_unique<Impl>(nextPowerOfTwo(newCapacity));

        for (size_t i = 0; i < oldImpl->capacity; ++i)
        {
            auto& entry = oldImpl->slots[i];
            if (entry.valid)
            {
                insertImplNoRehash(std::move(entry.key), std::move(entry.value));
            }
        }
    }
};

template <typename Key, typename Value>
using LinearHashMap = FlatHashMap<Key, Value, LinearProbing>;

template <typename Key, typename Value>
using RobinHoodHashMap = FlatHashMap<Key, Value, RobinHoodProbing>;

template <typename Key, typename Value>
using QuadraticHashMap = FlatHashMap<Key, Value, QuadraticProbing>;

template <typename Key, typename Value>
using DoubleHashingHashMap = FlatHashMap<Key, Value, DoubleHashingProbing<>>;

} // namespace casket