#pragma once
#include <vector>
#include <atomic>
#include <memory>
#include <functional>

namespace casket
{

template <typename T>
class LfObjectPool
{
public:
    LfObjectPool(const size_t capacity)
        : capacity_(capacity)
    {
        objectPool_.resize(capacity);
        slotAvailable_ = std::make_unique<std::atomic<bool>[]>(capacity);

        for (size_t i = 0; i < capacity; ++i)
        {
            slotAvailable_[i].store(true, std::memory_order_release);
        }
    }

    size_t capacity() const noexcept
    {
        return capacity_;
    }

    // Получить индекс свободного слота
    std::pair<size_t, T*> acquireSlot(std::function<void(T&)> initializer = nullptr)
    {
        for (size_t i = 0; i < capacity_; ++i)
        {
            size_t index = (nextIndex_.fetch_add(1, std::memory_order_relaxed)) % capacity_;

            bool expected = true;
            if (slotAvailable_[index].compare_exchange_strong(expected, false, std::memory_order_acquire,
                                                               std::memory_order_relaxed))
            {
                objectPool_[index].reset();
                if (initializer)
                {
                    initializer(objectPool_[index]);
                }
                return {index, &objectPool_[index]};
            }
        }
        return {capacity_, nullptr};
    }

    // Освободить слот
    void releaseSlot(size_t index)
    {
        if (index < capacity_)
        {
            slotAvailable_[index].store(true, std::memory_order_release);
        }
    }

    // Безопасный доступ к объекту
    T* getObject(size_t index)
    {
        if (index >= capacity_ || slotAvailable_[index].load(std::memory_order_acquire))
        {
            return nullptr;
        }
        return &objectPool_[index];
    }

    const T* getObject(size_t index) const
    {
        if (index >= capacity_ || slotAvailable_[index].load(std::memory_order_acquire))
        {
            return nullptr;
        }
        return &objectPool_[index];
    }

    // Метод для безопасного выполнения операций с объектом
    template <typename Func>
    void withObject(size_t index, Func func)
    {
        if (index < capacity_ && !slotAvailable_[index].load(std::memory_order_acquire))
        {
            func(objectPool_[index]);
        }
    }

    // Найти объект по предикату
    template <typename Predicate>
    T* findObject(Predicate pred)
    {
        for (size_t i = 0; i < capacity_; ++i)
        {
            if (!slotAvailable_[i].load(std::memory_order_acquire) && pred(objectPool_[i]))
            {
                return &objectPool_[i];
            }
        }
        return nullptr;
    }

    // Получить все активные объекты
    std::vector<T*> getActiveObjects()
    {
        std::vector<T*> active;
        for (size_t i = 0; i < capacity_; ++i)
        {
            if (!slotAvailable_[i].load(std::memory_order_acquire))
            {
                active.push_back(&objectPool_[i]);
            }
        }
        return active;
    }

private:
    std::vector<T> objectPool_;
    std::unique_ptr<std::atomic<bool>[]> slotAvailable_;
    std::atomic<size_t> nextIndex_{0};
    size_t capacity_;

};

} // namespace casket