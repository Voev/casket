#pragma once
#include <memory>
#include <vector>
#include <stack>
#include <functional>
#include <type_traits>
#include <cassert>
#include <cstddef>
#include <new>

namespace casket
{

template <typename T, typename Allocator = std::allocator<T>>
class ObjectPool
{
public:
    using value_type = T;
    using allocator_type = Allocator;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using size_type = typename std::allocator_traits<Allocator>::size_type;

    explicit ObjectPool(const Allocator& alloc = Allocator())
        : alloc_(alloc)
        , capacity_(0)
        , size_(0)
    {
    }

    explicit ObjectPool(size_type initialCapacity, const Allocator& alloc = Allocator())
        : alloc_(alloc)
        , capacity_(0)
        , size_(0)
    {
        reserve(initialCapacity);
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(ObjectPool&& other) noexcept
        : alloc_(std::move(other.alloc_))
        , memoryBlocks_(std::move(other.memoryBlocks_))
        , freeList_(std::move(other.freeList_))
        , capacity_(other.capacity_)
        , size_(other.size_)
    {
        other.capacity_ = 0;
        other.size_ = 0;
    }

    ObjectPool& operator=(ObjectPool&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            alloc_ = std::move(other.alloc_);
            memoryBlocks_ = std::move(other.memoryBlocks_);
            freeList_ = std::move(other.freeList_);
            capacity_ = other.capacity_;
            size_ = other.size_;
            other.capacity_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    ~ObjectPool() noexcept
    {
        clear();
    }

    template <typename... Args>
    T* create(Args&&... args)
    {
        if (freeList_.empty())
        {
            expand();
        }

        size_type index = freeList_.top();
        freeList_.pop();
        size_++;

        T* obj = getObjectPtr(index);
        try
        {
            std::allocator_traits<Allocator>::construct(alloc_, obj, std::forward<Args>(args)...);
        }
        catch (...)
        {
            freeList_.push(index);
            size_--;
            throw;
        }

        return obj;
    }

    void destroy(T* object)
    {
        assert(object != nullptr && "Cannot destroy null object");

        bool belongsToPool = false;
        size_type index = 0;

        for (size_type blockIdx = 0; blockIdx < memoryBlocks_.size(); ++blockIdx)
        {
            char* blockStart = memoryBlocks_[blockIdx].get();
            char* blockEnd = blockStart + blockSize_ * sizeof(T);

            if (reinterpret_cast<char*>(object) >= blockStart && reinterpret_cast<char*>(object) < blockEnd)
            {
                belongsToPool = true;
                index = blockIdx * blockSize_ + (reinterpret_cast<char*>(object) - blockStart) / sizeof(T);
                break;
            }
        }

        if (!belongsToPool)
        {
            throw std::invalid_argument("Object does not belong to this pool");
        }

        if (isFree(index))
        {
            throw std::invalid_argument("Object has already been destroyed");
        }

        std::allocator_traits<Allocator>::destroy(alloc_, object);
        freeList_.push(index);
        size_--;
    }

    void reserve(size_type newCapacity)
    {
        if (newCapacity <= capacity_)
            return;

        size_type blocks_needed = (newCapacity - capacity_ + blockSize_ - 1) / blockSize_;

        for (size_type i = 0; i < blocks_needed; ++i)
        {
            char* block = reinterpret_cast<char*>(std::allocator_traits<Allocator>::allocate(alloc_, blockSize_));

            memoryBlocks_.emplace_back(
                block, [this](char* ptr)
                { std::allocator_traits<Allocator>::deallocate(alloc_, reinterpret_cast<T*>(ptr), blockSize_); });

            for (size_type j = 0; j < blockSize_; ++j)
            {
                freeList_.push(capacity_ + i * blockSize_ + j);
            }
        }

        capacity_ += blocks_needed * blockSize_;
    }

    void clear() noexcept
    {
        for (size_type i = 0; i < capacity_; ++i)
        {
            if (!isFree(i))
            {
                T* obj = getObjectPtr(i);
                if (obj)
                {
                    std::allocator_traits<Allocator>::destroy(alloc_, obj);
                }
            }
        }

        memoryBlocks_.clear();
        while (!freeList_.empty())
        {
            freeList_.pop();
        }
        capacity_ = 0;
        size_ = 0;
    }

    size_type capacity() const noexcept
    {
        return capacity_;
    }

    size_type size() const noexcept
    {
        return size_;
    }

    bool empty() const noexcept
    {
        return size_ == 0;
    }

    allocator_type getAllocator() const noexcept
    {
        return alloc_;
    }

private:
    static constexpr size_type blockSize_ = 64;

    Allocator alloc_;
    std::vector<std::unique_ptr<char[], std::function<void(char*)>>> memoryBlocks_;
    std::stack<size_type> freeList_;
    size_type capacity_;
    size_type size_;

    void expand()
    {
        size_type newCapacity = capacity_ == 0 ? blockSize_ : capacity_ * 2;
        reserve(newCapacity);
    }

    T* getObjectPtr(size_type index)
    {
        if (index >= capacity_)
        {
            return nullptr;
        }

        size_type blockIndex = index / blockSize_;
        size_type objectIndex = index % blockSize_;

        if (blockIndex >= memoryBlocks_.size())
        {
            return nullptr;
        }

        char* block = memoryBlocks_[blockIndex].get();
        return reinterpret_cast<T*>(block + objectIndex * sizeof(T));
    }

    bool isFree(size_type index) const
    {
        auto temp = freeList_;
        while (!temp.empty())
        {
            if (temp.top() == index)
                return true;
            temp.pop();
        }
        return false;
    }
};

template <typename T, typename Allocator = std::allocator<T>, typename... Args>
std::shared_ptr<T> make_shared_from_pool(ObjectPool<T, Allocator>& pool, Args&&... args)
{
    return std::shared_ptr<T>(pool.create(std::forward<Args>(args)...), [&pool](T* obj) { pool.destroy(obj); });
}

template <typename T, typename Allocator = std::allocator<T>, typename... Args>
std::unique_ptr<T, std::function<void(T*)>> make_unique_from_pool(ObjectPool<T, Allocator>& pool, Args&&... args)
{
    return std::unique_ptr<T, std::function<void(T*)>>(pool.create(std::forward<Args>(args)...),
                                                       [&pool](T* obj) { pool.destroy(obj); });
}

} // namespace casket