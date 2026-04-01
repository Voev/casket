#pragma once
#include <atomic>
#include <cstddef>
#include <casket/lock_free/lf_list_node.hpp>

namespace casket
{

template <typename T>
class MPSCQueue
{
private:
    using Node = ListNode<T>;
    std::atomic<Node*> head_{nullptr};
    std::atomic<Node*> tail_{nullptr};
    std::atomic<size_t> size_{0};
    size_t max_size_;
    
public:
    explicit MPSCQueue(size_t max_size = SIZE_MAX) 
        : max_size_(max_size)
    {
        head_.store(nullptr, std::memory_order_relaxed);
        tail_.store(nullptr, std::memory_order_relaxed);
    }
    
    ~MPSCQueue()
    {
        clear();
    }
    
    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;
    MPSCQueue(MPSCQueue&&) = delete;
    MPSCQueue& operator=(MPSCQueue&&) = delete;

    // Добавление узла в очередь (узел должен быть выделен внешним пулом)
    bool push(Node* node)
    {
        if (!node) return false;
        if (full()) return false;
        
        node->next.store(nullptr, std::memory_order_relaxed);
        
        Node* prev_tail = tail_.exchange(node, std::memory_order_acq_rel);
        
        if (prev_tail == nullptr)
        {
            // Очередь была пуста
            head_.store(node, std::memory_order_release);
        }
        else
        {
            // Связываем предыдущий хвост с новым узлом
            prev_tail->next.store(node, std::memory_order_release);
        }
        
        size_.fetch_add(1, std::memory_order_release);
        return true;
    }
    
    // ========================================================================
    // Consumer API (должен вызываться только из одного потока)
    // ========================================================================
    
    // Извлечение узла (пользователь сам решает, что делать с узлом)
    Node* pop()
    {
        Node* head = head_.load(std::memory_order_acquire);
        
        if (head == nullptr)
        {
            return nullptr;  // Очередь пуста
        }
        
        Node* next = head->next.load(std::memory_order_acquire);
        
        if (next == nullptr)
        {
            // Возможно, это последний элемент
            Node* tail = tail_.load(std::memory_order_acquire);
            
            if (head != tail)
            {
                // Хвост обновился, значит есть новые элементы
                return nullptr;
            }
            
            // Пытаемся установить head и tail в nullptr
            if (head_.compare_exchange_strong(head, nullptr, 
                std::memory_order_acq_rel))
            {
                tail_.compare_exchange_strong(tail, nullptr, 
                    std::memory_order_acq_rel);
                size_.fetch_sub(1, std::memory_order_release);
                head->next.store(nullptr, std::memory_order_relaxed);
                return head;
            }
            return nullptr;  // CAS не удался
        }
        
        // Перемещаем голову на следующий узел
        if (head_.compare_exchange_strong(head, next, 
            std::memory_order_acq_rel))
        {
            size_.fetch_sub(1, std::memory_order_release);
            head->next.store(nullptr, std::memory_order_relaxed);
            return head;
        }
        
        return nullptr;  // CAS не удался
    }

    size_t size() const
    {
        return size_.load(std::memory_order_acquire);
    }
    
    bool empty() const
    {
        return size() == 0;
    }
    
    bool full() const
    {
        return size() >= max_size_;
    }
    
    size_t capacity() const
    {
        return max_size_;
    }
    
    void set_capacity(size_t max_size)
    {
        max_size_ = max_size;
    }
    
    void clear()
    {
        head_.store(nullptr, std::memory_order_release);
        tail_.store(nullptr, std::memory_order_release);
        size_.store(0, std::memory_order_release);
    }
};


} // namespace casket