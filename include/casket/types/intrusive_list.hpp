#pragma once

#include <iterator>
#include <type_traits>

namespace casket
{

template <typename T>
class IntrusiveListNode
{
public:
    IntrusiveListNode() = default;

    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;

    T* next = nullptr;
    T* prev = nullptr;

protected:
    ~IntrusiveListNode() = default;
};

template <typename T>
class IntrusiveList final
{
    static_assert(std::is_same_v<decltype(T::next), T*>, "T must have 'next' member of type T*");
    static_assert(std::is_same_v<decltype(T::prev), T*>, "T must have 'prev' member of type T*");

public:
    class Iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        explicit Iterator(T* ptr = nullptr)
            : current_(ptr)
        {
        }

        reference operator*() const
        {
            return *current_;
        }
        pointer operator->() const
        {
            return current_;
        }

        Iterator& operator++()
        {
            current_ = current_->next;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator& operator--()
        {
            current_ = current_->prev;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const
        {
            return current_ == other.current_;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

    private:
        T* current_;
    };

    class ConstIterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = const T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        explicit ConstIterator(const T* ptr = nullptr)
            : current_(ptr)
        {
        }

        reference operator*() const
        {
            return *current_;
        }
        pointer operator->() const
        {
            return current_;
        }

        ConstIterator& operator++()
        {
            current_ = current_->next;
            return *this;
        }

        ConstIterator operator++(int)
        {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        ConstIterator& operator--()
        {
            current_ = current_->prev;
            return *this;
        }

        ConstIterator operator--(int)
        {
            ConstIterator tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const ConstIterator& other) const
        {
            return current_ == other.current_;
        }

        bool operator!=(const ConstIterator& other) const
        {
            return !(*this == other);
        }

    private:
        const T* current_;
    };

    IntrusiveList() = default;

    ~IntrusiveList()
    {
        clear();
    }

    void push_front(T& item)
    {
        item.next = head_;
        item.prev = nullptr;

        if (head_)
        {
            head_->prev = &item;
        }
        else
        {
            tail_ = &item;
        }

        head_ = &item;
        ++size_;
    }

    void push_back(T& item)
    {
        item.prev = tail_;
        item.next = nullptr;

        if (tail_)
        {
            tail_->next = &item;
        }
        else
        {
            head_ = &item;
        }

        tail_ = &item;
        ++size_;
    }

    void remove(T& item)
    {
        if (item.prev != nullptr || item.next != nullptr || head_ == &item)
        {
            if (item.prev)
            {
                item.prev->next = item.next;
            }
            else
            {
                head_ = item.next;
            }

            if (item.next)
            {
                item.next->prev = item.prev;
            }
            else
            {
                tail_ = item.prev;
            }

            item.next = nullptr;
            item.prev = nullptr;
            --size_;
        }
    }

    void clear()
    {
        while (head_)
        {
            auto* next = head_->next;
            head_->next = nullptr;
            head_->prev = nullptr;
            head_ = next;
        }
        tail_ = nullptr;
        size_ = 0;
    }

    T& front()
    {
        return *head_;
    }
    const T& front() const
    {
        return *head_;
    }
    T& back()
    {
        return *tail_;
    }
    const T& back() const
    {
        return *tail_;
    }

    Iterator begin()
    {
        return Iterator(head_);
    }
    Iterator end()
    {
        return Iterator(nullptr);
    }
    ConstIterator begin() const
    {
        return ConstIterator(head_);
    }
    ConstIterator end() const
    {
        return ConstIterator(nullptr);
    }
    ConstIterator cbegin() const
    {
        return ConstIterator(head_);
    }
    ConstIterator cend() const
    {
        return ConstIterator(nullptr);
    }

    size_t size() const
    {
        return size_;
    }
    bool empty() const
    {
        return size_ == 0;
    }

private:
    T* head_ = nullptr;
    T* tail_ = nullptr;
    size_t size_ = 0;
};

} // namespace casket