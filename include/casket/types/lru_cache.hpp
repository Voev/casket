#pragma once

#include <casket/nonstd/string_view.hpp>
#include <casket/nonstd/optional.hpp>
#include <unordered_map>
#include <memory>
#include <functional>
#include <casket/types/intrusive_list.hpp>
#include <casket/memory/object_pool.hpp>

namespace casket
{

template <typename KeyType, typename ValueType>
class LRUCache
{
private:
    struct CacheNode : public IntrusiveListNode<CacheNode>
    {
        KeyType key;
        ValueType value;

        template <typename K, typename V>
        CacheNode(K&& k, V&& v)
            : key(std::forward<K>(k))
            , value(std::forward<V>(v))
        {
        }
    };

public:
    explicit LRUCache(size_t maxSize = 10)
        : maxSize_(maxSize)
        , nodePool_(maxSize) // Пул того же размера что и кэш
    {
        cacheMap_.reserve(maxSize);
    }

    ~LRUCache()
    {
        clear();
    }

    /// @brief Сохраняет значение в кэше
    bool put(KeyType key, ValueType value)
    {
        if (maxSize_ == 0)
            return false;

        auto it = cacheMap_.find(key);
        if (it != cacheMap_.end())
        {
            // Обновляем существующий элемент
            it->second->value = std::move(value);
            promote(it->second);
            return true;
        }

        // Проверяем необходимость вытеснения
        if (cacheList_.size() >= maxSize_)
        {
            // Вытесняем самый старый элемент
            evictOldest();
        }

        // Создаем новый узел через пул
        CacheNode* node = nodePool_.construct(std::move(key), std::move(value));

        // Добавляем в начало списка
        cacheList_.push_front(*node);
        cacheMap_.emplace(node->key, node);

        return true;
    }

    /// @brief Перегрузки для обратной совместимости
    bool put(const KeyType& key, const ValueType& value)
    {
        return put(KeyType(key), ValueType(value));
    }

    bool put(const KeyType& key, ValueType&& value)
    {
        return put(KeyType(key), std::move(value));
    }

    /// @brief Ищет значение и возвращает указатель
    ValueType* find(const KeyType& key)
    {
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end())
            return nullptr;

        promote(it->second);
        return &it->second->value;
    }

    /// @brief Возвращает optional с ссылкой
    nonstd::optional<std::reference_wrapper<ValueType>> get(const KeyType& key)
    {
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end())
            return std::nullopt;

        promote(it->second);
        return std::ref(it->second->value);
    }

    /// @brief Извлекает значение с move-семантикой
    nonstd::optional<ValueType> extract(const KeyType& key)
    {
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end())
            return std::nullopt;

        CacheNode* node = it->second;
        ValueType result = std::move(node->value);

        removeNode(node);
        return result;
    }

    /// @brief Проверяет наличие ключа
    bool contains(const KeyType& key) const
    {
        return cacheMap_.find(key) != cacheMap_.end();
    }

    /// @brief Удаляет элемент по ключу
    bool erase(const KeyType& key)
    {
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end())
            return false;

        removeNode(it->second);
        return true;
    }

    /// @brief Очищает кэш
    void clear() noexcept
    {
        // Удаляем все узлы из списка и возвращаем в пул
        while (!cacheList_.empty())
        {
            CacheNode* node = &cacheList_.back();
            cacheList_.remove(*node);
            nodePool_.destroy(node);
        }
        cacheMap_.clear();
    }

    size_t size() const noexcept
    {
        return cacheList_.size();
    }
    size_t capacity() const noexcept
    {
        return maxSize_;
    }
    bool empty() const noexcept
    {
        return cacheList_.empty();
    }

    /// @brief Изменяет максимальный размер
    void resize(size_t newMaxSize)
    {
        maxSize_ = newMaxSize;
        cacheMap_.reserve(newMaxSize);

        // Вытесняем лишние элементы
        while (cacheList_.size() > maxSize_)
        {
            evictOldest();
        }

        // Изменяем размер пула (опционально)
        nodePool_.reserve(newMaxSize);
    }

    /// @brief Статистика использования
    size_t pool_size() const noexcept
    {
        return nodePool_.size();
    }
    size_t pool_capacity() const noexcept
    {
        return nodePool_.capacity();
    }
    size_t pool_free_count() const noexcept
    {
        return nodePool_.free_count();
    }

    /// @brief Оптимизация: предварительное выделение
    void reserve(size_t capacity)
    {
        maxSize_ = capacity;
        cacheMap_.reserve(capacity);
        nodePool_.reserve(capacity);
    }

private:
    void promote(CacheNode* node)
    {
        cacheList_.remove(*node);
        cacheList_.push_front(*node);
    }

    void removeNode(CacheNode* node)
    {
        cacheList_.remove(*node);
        cacheMap_.erase(node->key);
        nodePool_.destroy(node);
    }

    void evictOldest()
    {
        if (cacheList_.empty())
        {
            return;
        }

        CacheNode* node = &cacheList_.back();
        cacheList_.remove(*node);
        cacheMap_.erase(node->key);
        nodePool_.destroy(node);
    }

    size_t maxSize_;
    IntrusiveList<CacheNode> cacheList_;
    std::unordered_map<KeyType, CacheNode*> cacheMap_;
    ObjectPool<CacheNode> nodePool_;
};

} // namespace casket