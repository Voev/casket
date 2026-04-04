#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

namespace casket
{

template <typename T>
class DirectMemoryPolicy
{
public:
    T* create()
    {
        return new T();
    }

    void destroy(T* ptr)
    {
        delete ptr;
    }

    void clear()
    {
        // Nothing to do
    }
};

template <typename T, size_t PoolSize = 10000>
class PoolMemoryPolicy
{
private:
    union Slot
    {
        T object;
        size_t nextFree;

        Slot()
            : nextFree(0)
        {
        }
        ~Slot()
        {
        }
    };

    std::array<Slot, PoolSize> pool_;
    size_t freeHead_;
    size_t usedCount_;

public:
    PoolMemoryPolicy()
        : freeHead_(0)
        , usedCount_(0)
    {
        for (size_t i = 0; i < PoolSize - 1; ++i)
        {
            pool_[i].nextFree = i + 1;
        }
        pool_[PoolSize - 1].nextFree = PoolSize;
    }

    T* create()
    {
        if (freeHead_ >= PoolSize)
        {
            return nullptr;
        }

        size_t index = freeHead_;
        freeHead_ = pool_[freeHead_].nextFree;
        usedCount_++;

        T* ptr = &pool_[index].object;
        new (ptr) T();
        return ptr;
    }

    void destroy(T* ptr)
    {
        if (!ptr)
            return;

        ptr->~T();
        size_t index = reinterpret_cast<Slot*>(ptr) - pool_.data();
        pool_[index].nextFree = freeHead_;
        freeHead_ = index;
        usedCount_--;
    }

    void clear()
    {
        for (size_t i = 0; i < PoolSize; ++i)
        {
            if (isUsed(i))
            {
                pool_[i].object.~T();
            }
        }
        freeHead_ = 0;
        usedCount_ = 0;
        for (size_t i = 0; i < PoolSize - 1; ++i)
        {
            pool_[i].nextFree = i + 1;
        }
        pool_[PoolSize - 1].nextFree = PoolSize;
    }

    size_t usedCount() const
    {
        return usedCount_;
    }

private:
    bool isUsed(size_t index) const
    {
        size_t current = freeHead_;
        while (current < PoolSize)
        {
            if (current == index)
                return false;
            current = pool_[current].nextFree;
        }
        return true;
    }
};

template <typename T>
class VectorMemoryPolicy
{
private:
    struct Block
    {
        T object;
        bool used;
    };

    std::vector<Block> blocks_;
    std::vector<size_t> freeList_;

public:
    T* create()
    {
        if (!freeList_.empty())
        {
            size_t index = freeList_.back();
            freeList_.pop_back();
            blocks_[index].used = true;
            return &blocks_[index].object;
        }

        size_t index = blocks_.size();
        blocks_.emplace_back();
        blocks_[index].used = true;
        new (&blocks_[index].object) T();
        return &blocks_[index].object;
    }

    void destroy(T* ptr)
    {
        if (!ptr)
            return;

        size_t index = reinterpret_cast<Block*>(ptr) - blocks_.data();
        blocks_[index].object.~T();
        blocks_[index].used = false;
        freeList_.push_back(index);
    }

    void clear()
    {
        for (auto& block : blocks_)
        {
            if (block.used)
            {
                block.object.~T();
            }
        }
        blocks_.clear();
        freeList_.clear();
    }
};

template <typename T>
class SharedMemoryPolicy
{
private:
    std::vector<std::shared_ptr<T>> storage_;
    std::vector<size_t> freeList_;

public:
    T* create()
    {
        if (!freeList_.empty())
        {
            size_t index = freeList_.back();
            freeList_.pop_back();
            return storage_[index].get();
        }

        storage_.push_back(std::make_shared<T>());
        return storage_.back().get();
    }

    void destroy(T* ptr)
    {
        if (!ptr)
            return;

        for (size_t i = 0; i < storage_.size(); ++i)
        {
            if (storage_[i].get() == ptr)
            {
                storage_[i].reset();
                freeList_.push_back(i);
                break;
            }
        }
    }

    void clear()
    {
        storage_.clear();
        freeList_.clear();
    }
};

} // namespace casket