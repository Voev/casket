#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <cassert>

namespace casket
{

template <typename T, size_t Capacity = 10000>
class ObjectPool
{
private:
    union Slot
    {
        T object;
        size_t next_free;

        Slot()
            : next_free(0)
        {
        }
        ~Slot()
        {
        }
    };

    std::array<Slot, Capacity> pool_;
    size_t free_head_;
    size_t used_count_;
    std::atomic<bool> initialized_;

public:
    ObjectPool()
        : free_head_(0)
        , used_count_(0)
        , initialized_(false)
    {
        // Инициализация списка свободных слотов
        for (size_t i = 0; i < Capacity - 1; ++i)
        {
            pool_[i].next_free = i + 1;
        }
        pool_[Capacity - 1].next_free = Capacity; // Используем Capacity как маркер конца
        initialized_.store(true, std::memory_order_release);
    }

    ~ObjectPool()
    {
        // Деструктурируем только используемые объекты
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (size_t i = 0; i < Capacity; ++i)
            {
                if (is_used(i))
                {
                    pool_[i].object.~T();
                }
            }
        }
    }

    // Запрещаем копирование
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Разрешаем перемещение
    ObjectPool(ObjectPool&& other) noexcept
        : free_head_(other.free_head_)
        , used_count_(other.used_count_)
    {
        pool_ = std::move(other.pool_);
        other.free_head_ = Capacity;
        other.used_count_ = 0;
    }

    T* allocate()
    {
        assert(initialized_.load(std::memory_order_acquire));

        if (free_head_ >= Capacity)
        {
            return nullptr; // Пул полон
        }

        size_t index = free_head_;
        free_head_ = pool_[free_head_].next_free;
        used_count_++;

        // placement new для создания объекта
        T* ptr = &pool_[index].object;
        new (ptr) T();

        return ptr;
    }

    template <typename... Args>
    T* allocate(Args&&... args)
    {
        if (free_head_ >= Capacity)
        {
            return nullptr;
        }

        size_t index = free_head_;
        free_head_ = pool_[free_head_].next_free;
        used_count_++;

        T* ptr = &pool_[index].object;
        new (ptr) T(std::forward<Args>(args)...);

        return ptr;
    }

    void deallocate(T* ptr)
    {
        if (!ptr)
            return;

        size_t index = get_index(ptr);
        if (index >= Capacity)
            return;

        ptr->~T(); // Явный вызов деструктора

        pool_[index].next_free = free_head_;
        free_head_ = index;
        used_count_--;
    }

    size_t used_count() const
    {
        return used_count_;
    }
    size_t capacity() const
    {
        return Capacity;
    }
    bool is_full() const
    {
        return free_head_ >= Capacity;
    }

private:
    size_t get_index(const T* ptr) const
    {
        const Slot* slot = reinterpret_cast<const Slot*>(ptr);
        size_t index = slot - pool_.data();
        return index;
    }

    bool is_used(size_t index) const
    {
        // Проверяем, не находится ли индекс в списке свободных
        size_t current = free_head_;
        while (current < Capacity)
        {
            if (current == index)
                return false;
            current = pool_[current].next_free;
        }
        return true;
    }
};

} // namespace casket