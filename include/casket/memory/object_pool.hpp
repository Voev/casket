#pragma once

#include <memory>
#include <vector>
#include <stack>
#include <functional>
#include <type_traits>
#include <cassert>

namespace casket
{

template <typename T, typename Allocator = std::allocator<T>>
class ObjectPool
{
private:
    struct Block
    {
        T* memory;
        size_t capacity;
        size_t used;

        Block(T* mem, size_t cap)
            : memory(mem)
            , capacity(cap)
            , used(0)
        {
        }
    };

public:
    explicit ObjectPool(size_t chunkSize = 64, Allocator allocator = Allocator())
        : chunkSize_(chunkSize)
        , allocator_(allocator)
        , freeList_(nullptr)
    {
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
        allocateChunk();
    }

    ~ObjectPool()
    {
        clear();
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(ObjectPool&& other) noexcept
        : chunkSize_(other.chunkSize_)
        , allocator_(std::move(other.allocator_))
        , blocks_(std::move(other.blocks_))
        , freeList_(other.freeList_)
    {
        other.freeList_ = nullptr;
        other.blocks_.clear();
    }

    ObjectPool& operator=(ObjectPool&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            chunkSize_ = other.chunkSize_;
            allocator_ = std::move(other.allocator_);
            blocks_ = std::move(other.blocks_);
            freeList_ = other.freeList_;
            other.freeList_ = nullptr;
            other.blocks_.clear();
        }
        return *this;
    }

    template <typename... Args>
    T* construct(Args&&... args)
    {
        T* object = acquireRaw();
        try
        {
            new (object) T(std::forward<Args>(args)...);
        }
        catch (...)
        {
            releaseRaw(object);
            throw;
        }
        return object;
    }

    void destroy(T* object) noexcept
    {
        if (object)
        {
            object->~T();
            releaseRaw(object);
        }
    }

    template <typename... Args>
    std::unique_ptr<T, std::function<void(T*)>> make_unique(Args&&... args)
    {
        T* object = construct(std::forward<Args>(args)...);
        return std::unique_ptr<T, std::function<void(T*)>>(object, [this](T* ptr) { this->destroy(ptr); });
    }

    template <typename... Args>
    std::shared_ptr<T> make_shared(Args&&... args)
    {
        T* object = construct(std::forward<Args>(args)...);
        return std::shared_ptr<T>(object, [this](T* ptr) { this->destroy(ptr); });
    }

    size_t size() const noexcept
    {
        size_t total = 0;
        for (const auto& block : blocks_)
        {
            total += block.used;
        }
        return total - freeCount_;
    }

    size_t capacity() const noexcept
    {
        size_t total = 0;
        for (const auto& block : blocks_)
        {
            total += block.capacity;
        }
        return total;
    }

    size_t free_count() const noexcept
    {
        return freeCount_;
    }

    bool empty() const noexcept
    {
        return size() == 0;
    }

    void clear() noexcept
    {
        // Деструктурируем все активные объекты
        for (auto& block : blocks_)
        {
            for (size_t i = 0; i < block.used; ++i)
            {
                T* object = block.memory + i;
                if (!isInFreeList(object))
                {
                    object->~T();
                }
            }
        }

        // Освобождаем всю память
        for (auto& block : blocks_)
        {
            allocator_.deallocate(block.memory, block.capacity);
        }
        blocks_.clear();
        freeList_ = nullptr;
        freeCount_ = 0;
    }

    void reserve(size_t capacity)
    {
        while (this->capacity() < capacity)
        {
            allocateChunk();
        }
    }

    void shrink_to_fit()
    {
        // Находим блоки, которые полностью свободны
        std::vector<Block> newBlocks;
        newBlocks.reserve(blocks_.size());

        for (auto& block : blocks_)
        {
            size_t activeInBlock = 0;
            for (size_t i = 0; i < block.used; ++i)
            {
                T* object = block.memory + i;
                if (!isInFreeList(object))
                {
                    ++activeInBlock;
                }
            }

            if (activeInBlock > 0)
            {
                newBlocks.push_back(std::move(block));
            }
            else
            {
                allocator_.deallocate(block.memory, block.capacity);
            }
        }

        blocks_ = std::move(newBlocks);
        rebuildFreeList();
    }

private:
    union FreeNode
    {
        T object;
        FreeNode* next;

        FreeNode()
            : next(nullptr)
        {
        }
        ~FreeNode()
        {
        }
    };

    T* acquireRaw()
    {
        if (freeList_ == nullptr)
        {
            allocateChunk();
        }

        FreeNode* node = freeList_;
        freeList_ = freeList_->next;
        --freeCount_;

        return reinterpret_cast<T*>(node);
    }

    void releaseRaw(T* object) noexcept
    {
        FreeNode* node = reinterpret_cast<FreeNode*>(object);
        node->next = freeList_;
        freeList_ = node;
        ++freeCount_;
    }

    void allocateChunk()
    {
        T* memory = allocator_.allocate(chunkSize_);
        blocks_.emplace_back(memory, chunkSize_);
        Block& block = blocks_.back();

        // Добавляем все объекты нового блока в free list
        for (size_t i = 0; i < chunkSize_; ++i)
        {
            T* object = memory + i;
            releaseRaw(object);
            block.used++;
        }
    }

    bool isInFreeList(T* object) const noexcept
    {
        FreeNode* current = freeList_;
        while (current)
        {
            if (reinterpret_cast<T*>(current) == object)
            {
                return true;
            }
            current = current->next;
        }
        return false;
    }

    void rebuildFreeList()
    {
        freeList_ = nullptr;
        freeCount_ = 0;

        for (auto& block : blocks_)
        {
            for (size_t i = 0; i < block.used; ++i)
            {
                T* object = block.memory + i;
                // Проверяем, свободен ли объект
                bool isFree = true;

                // Здесь должна быть логика определения свободных объектов
                // В реальной реализации это требует отслеживания состояния

                if (isFree)
                {
                    releaseRaw(object);
                }
            }
        }
    }

    size_t chunkSize_;
    Allocator allocator_;
    std::vector<Block> blocks_;
    FreeNode* freeList_;
    size_t freeCount_ = 0;
};

// Специализация для простых типов (POD)
template <typename T, typename Allocator = std::allocator<T>>
class SimpleObjectPool
{
public:
    explicit SimpleObjectPool(size_t chunkSize = 1024)
        : chunkSize_(chunkSize)
        , freeList_(nullptr)
    {
        allocateChunk();
    }

    ~SimpleObjectPool()
    {
        clear();
    }

    T* acquire()
    {
        if (freeList_ == nullptr)
        {
            allocateChunk();
        }

        T* object = freeList_;
        freeList_ = *reinterpret_cast<T**>(freeList_);
        return object;
    }

    void release(T* object) noexcept
    {
        *reinterpret_cast<T**>(object) = freeList_;
        freeList_ = object;
    }

    void clear() noexcept
    {
        for (auto chunk : chunks_)
        {
            ::operator delete(chunk);
        }
        chunks_.clear();
        freeList_ = nullptr;
    }

private:
    void allocateChunk()
    {
        char* chunk = static_cast<char*>(::operator new(chunkSize_ * sizeof(T)));
        chunks_.push_back(chunk);

        // Связываем все объекты в free list
        for (size_t i = 0; i < chunkSize_; ++i)
        {
            T* object = reinterpret_cast<T*>(chunk + i * sizeof(T));
            release(object);
        }
    }

    size_t chunkSize_;
    std::vector<char*> chunks_;
    T* freeList_;
};

} // namespace casket