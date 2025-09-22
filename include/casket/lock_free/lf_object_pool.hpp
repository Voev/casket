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
        objects_pool_.resize(capacity);
        slot_available_ = std::make_unique<std::atomic<bool>[]>(capacity);

        for (size_t i = 0; i < capacity; ++i)
        {
            slot_available_[i].store(true, std::memory_order_release);
        }
    }

    size_t capacity() const noexcept
    {
        return capacity_;
    }

    // Получить индекс свободного слота
    std::pair<size_t, T*> acquire_slot(std::function<void(T&)> initializer = nullptr)
    {
        for (size_t i = 0; i < capacity_; ++i)
        {
            size_t index = (next_index_.fetch_add(1, std::memory_order_relaxed)) % capacity_;

            bool expected = true;
            if (slot_available_[index].compare_exchange_strong(expected, false, std::memory_order_acquire,
                                                               std::memory_order_relaxed))
            {
                objects_pool_[index].reset();
                if (initializer)
                {
                    initializer(objects_pool_[index]);
                }
                return {index, &objects_pool_[index]};
            }
        }
        return {capacity_, nullptr};
    }

    // Освободить слот
    void release_slot(size_t index)
    {
        if (index < capacity_)
        {
            slot_available_[index].store(true, std::memory_order_release);
        }
    }

    // Безопасный доступ к объекту
    T* get_object(size_t index)
    {
        if (index >= capacity_ || slot_available_[index].load(std::memory_order_acquire))
        {
            return nullptr;
        }
        return &objects_pool_[index];
    }

    const T* get_object(size_t index) const
    {
        if (index >= capacity_ || slot_available_[index].load(std::memory_order_acquire))
        {
            return nullptr;
        }
        return &objects_pool_[index];
    }

    // Метод для безопасного выполнения операций с объектом
    template <typename Func>
    void with_object(size_t index, Func func)
    {
        if (index < capacity_ && !slot_available_[index].load(std::memory_order_acquire))
        {
            func(objects_pool_[index]);
        }
    }

    // Найти объект по предикату
    template <typename Predicate>
    T* find_object(Predicate pred)
    {
        for (size_t i = 0; i < capacity_; ++i)
        {
            if (!slot_available_[i].load(std::memory_order_acquire) && pred(objects_pool_[i]))
            {
                return &objects_pool_[i];
            }
        }
        return nullptr;
    }

    // Получить все активные объекты
    std::vector<T*> get_active_objects()
    {
        std::vector<T*> active;
        for (size_t i = 0; i < capacity_; ++i)
        {
            if (!slot_available_[i].load(std::memory_order_acquire))
            {
                active.push_back(&objects_pool_[i]);
            }
        }
        return active;
    }

private:
    std::vector<T> objects_pool_;
    std::unique_ptr<std::atomic<bool>[]> slot_available_;
    std::atomic<size_t> next_index_{0};
    size_t capacity_;

};

} // namespace casket