#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

namespace casket
{

/// @brief Base node for intrusive list elements
/// @tparam T Derived type (CRTP pattern)
/// 
/// Embed this node into classes that need to be stored in an IntrusiveList.
/// Provides next/prev pointers with copy semantics disabled to prevent
/// accidental duplication of links.
template <typename T>
class IntrusiveListNode
{
public:
    IntrusiveListNode() = default;
    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;

    T* next = nullptr;  ///< Pointer to next element in the list
    T* prev = nullptr;  ///< Pointer to previous element in the list

protected:
    ~IntrusiveListNode() = default;
};

/// @brief Intrusive doubly-linked list with zero memory overhead
/// @tparam T Element type (must have next/prev members of type T*)
/// 
/// Provides O(1) insertion, removal, and access operations without memory allocations.
/// Elements must be managed externally - the list does not own or copy them.
/// 
/// Features:
/// - No memory allocations
/// - Constant-time operations
/// - Bidirectional iteration
/// - Move semantics support
/// - STL-compatible iterators
template <typename T>
class IntrusiveList final
{
    static_assert(std::is_same_v<decltype(T::next), T*>, "T must have 'next' member");
    static_assert(std::is_same_v<decltype(T::prev), T*>, "T must have 'prev' member");

public:
    /// @brief Bidirectional iterator for non-const access
    class Iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        /// @brief Construct iterator pointing to an element
        /// @param[in] ptr Pointer to element (nullptr for end iterator)
        explicit Iterator(T* ptr = nullptr) : current_(ptr) {}

        /// @brief Dereference operator
        /// @return Reference to current element
        reference operator*() const { return *current_; }

        /// @brief Member access operator
        /// @return Pointer to current element
        pointer operator->() const { return current_; }

        /// @brief Pre-increment (forward)
        /// @return Reference to this iterator
        Iterator& operator++() { current_ = current_->next; return *this; }

        /// @brief Post-increment (forward)
        /// @return Copy of iterator before increment
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

        /// @brief Pre-decrement (backward)
        /// @return Reference to this iterator
        Iterator& operator--() { current_ = current_->prev; return *this; }

        /// @brief Post-decrement (backward)
        /// @return Copy of iterator before decrement
        Iterator operator--(int) { Iterator tmp = *this; --(*this); return tmp; }

        /// @brief Equality comparison
        /// @param[in] other Iterator to compare with
        /// @return true if iterators point to same element
        bool operator==(const Iterator& other) const { return current_ == other.current_; }

        /// @brief Inequality comparison
        /// @param[in] other Iterator to compare with
        /// @return true if iterators point to different elements
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        T* current_;
    };

    /// @brief Bidirectional iterator for const access
    class ConstIterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = const T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        /// @brief Construct const iterator pointing to an element
        /// @param[in] ptr Pointer to element (nullptr for end iterator)
        explicit ConstIterator(const T* ptr = nullptr) : current_(ptr) {}

        /// @brief Dereference operator
        /// @return Const reference to current element
        reference operator*() const { return *current_; }

        /// @brief Member access operator
        /// @return Const pointer to current element
        pointer operator->() const { return current_; }

        /// @brief Pre-increment (forward)
        /// @return Reference to this iterator
        ConstIterator& operator++() { current_ = current_->next; return *this; }

        /// @brief Post-increment (forward)
        /// @return Copy of iterator before increment
        ConstIterator operator++(int) { ConstIterator tmp = *this; ++(*this); return tmp; }

        /// @brief Pre-decrement (backward)
        /// @return Reference to this iterator
        ConstIterator& operator--() { current_ = current_->prev; return *this; }

        /// @brief Post-decrement (backward)
        /// @return Copy of iterator before decrement
        ConstIterator operator--(int) { ConstIterator tmp = *this; --(*this); return tmp; }

        /// @brief Equality comparison
        /// @param[in] other Iterator to compare with
        /// @return true if iterators point to same element
        bool operator==(const ConstIterator& other) const { return current_ == other.current_; }

        /// @brief Inequality comparison
        /// @param[in] other Iterator to compare with
        /// @return true if iterators point to different elements
        bool operator!=(const ConstIterator& other) const { return !(*this == other); }

    private:
        const T* current_;
    };

    /// @brief Construct empty list
    IntrusiveList() = default;

    /// @brief Move constructor
    /// @param[in,out] other List to move from (becomes empty)
    IntrusiveList(IntrusiveList&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_)
    {
        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;
    }

    /// @brief Move assignment operator
    /// @param[in,out] other List to move from (becomes empty)
    /// @return Reference to this list
    IntrusiveList& operator=(IntrusiveList&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            other.head_ = other.tail_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    /// @brief Copy constructor (deleted)
    IntrusiveList(const IntrusiveList&) = delete;

    /// @brief Copy assignment operator (deleted)
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    /// @brief Destructor - unlinks all elements
    ~IntrusiveList() { clear(); }

    // ========================================================================
    // Element Access
    // ========================================================================

    /// @brief Get first element without removing
    /// @return Pointer to first element, or nullptr if list is empty
    T* front() { return head_; }

    /// @brief Get first element without removing (const version)
    /// @return Const pointer to first element, or nullptr if list is empty
    const T* front() const { return head_; }

    /// @brief Get last element without removing
    /// @return Pointer to last element, or nullptr if list is empty
    T* back() { return tail_; }

    /// @brief Get last element without removing (const version)
    /// @return Const pointer to last element, or nullptr if list is empty
    const T* back() const { return tail_; }

    // ========================================================================
    // Capacity
    // ========================================================================

    /// @brief Check if list is empty
    /// @return true if list contains no elements
    bool empty() const { return size_ == 0; }

    /// @brief Get number of elements in the list
    /// @return Number of elements
    size_t size() const { return size_; }

    // ========================================================================
    // Modifiers
    // ========================================================================

    /// @brief Insert element at front
    /// @param[in,out] item Element to insert (must not be in another list)
    /// @complexity O(1)
    void push_front(T& item)
    {
        item.next = head_;
        item.prev = nullptr;

        if (head_)
            head_->prev = &item;
        else
            tail_ = &item;

        head_ = &item;
        ++size_;
    }

    /// @brief Insert element at back
    /// @param[in,out] item Element to insert (must not be in another list)
    /// @complexity O(1)
    void push_back(T& item)
    {
        item.prev = tail_;
        item.next = nullptr;

        if (tail_)
            tail_->next = &item;
        else
            head_ = &item;

        tail_ = &item;
        ++size_;
    }

    /// @brief Remove and return front element
    /// @return Pointer to removed element, or nullptr if list is empty
    /// @complexity O(1)
    T* pop_front()
    {
        if (!head_) return nullptr;
        T* item = head_;
        remove(*item);
        return item;
    }

    /// @brief Remove and return back element
    /// @return Pointer to removed element, or nullptr if list is empty
    /// @complexity O(1)
    T* pop_back()
    {
        if (!tail_) return nullptr;
        T* item = tail_;
        remove(*item);
        return item;
    }

    /// @brief Remove element from list
    /// @param[in,out] item Element to remove (if in list)
    /// @complexity O(1)
    void remove(T& item)
    {
        if (item.prev != nullptr || item.next != nullptr || head_ == &item)
        {
            if (item.prev)
                item.prev->next = item.next;
            else
                head_ = item.next;

            if (item.next)
                item.next->prev = item.prev;
            else
                tail_ = item.prev;

            item.next = nullptr;
            item.prev = nullptr;
            --size_;
        }
    }

    /// @brief Remove element by pointer
    /// @param[in,out] item Pointer to element to remove
    /// @complexity O(1)
    void remove(T* item)
    {
        if (item) remove(*item);
    }

    /// @brief Remove all elements from list
    /// @note Does not delete elements, only unlinks them
    /// @complexity O(n)
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

    // ========================================================================
    // List Operations
    // ========================================================================

    /// @brief Move all elements from another list into this list
    /// @param[in,out] other List to move elements from (becomes empty)
    /// @complexity O(1)
    void splice(IntrusiveList& other)
    {
        if (other.empty()) return;

        if (empty())
        {
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
        }
        else
        {
            tail_->next = other.head_;
            other.head_->prev = tail_;
            tail_ = other.tail_;
            size_ += other.size_;
        }

        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;
    }

    /// @brief Move a single element from another list to this list
    /// @param[in,out] other List containing the element
    /// @param[in,out] item Element to move
    /// @complexity O(1)
    void splice(IntrusiveList& other, T& item)
    {
        other.remove(item);
        push_back(item);
    }

    // ========================================================================
    // Queue Operations (Convenience Aliases)
    // ========================================================================

    /// @brief Insert element at front (alias for push_front)
    /// @param[in,out] item Element to insert
    void push(T& item) { push_front(item); }

    /// @brief Insert element at back (alias for push_back)
    /// @param[in,out] item Element to insert
    void enqueue(T& item) { push_back(item); }

    /// @brief Remove and return front element (alias for pop_front)
    /// @return Pointer to removed element, or nullptr if list is empty
    T* dequeue() { return pop_front(); }

    // ========================================================================
    // Iterators
    // ========================================================================

    /// @brief Get iterator to first element
    /// @return Iterator pointing to first element
    Iterator begin() { return Iterator(head_); }

    /// @brief Get iterator to end (past-the-last)
    /// @return Iterator representing end of list
    Iterator end() { return Iterator(nullptr); }

    /// @brief Get const iterator to first element
    /// @return Const iterator pointing to first element
    ConstIterator begin() const { return ConstIterator(head_); }

    /// @brief Get const iterator to end
    /// @return Const iterator representing end of list
    ConstIterator end() const { return ConstIterator(nullptr); }

    /// @brief Get const iterator to first element
    /// @return Const iterator pointing to first element
    ConstIterator cbegin() const { return ConstIterator(head_); }

    /// @brief Get const iterator to end
    /// @return Const iterator representing end of list
    ConstIterator cend() const { return ConstIterator(nullptr); }

    // ========================================================================
    // Reverse Iterators
    // ========================================================================

    /// @brief Reverse iterator for non-const access
    class ReverseIterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        /// @brief Construct reverse iterator pointing to an element
        /// @param[in] ptr Pointer to element (nullptr for end iterator)
        explicit ReverseIterator(T* ptr = nullptr) : current_(ptr) {}

        /// @brief Dereference operator
        /// @return Reference to current element
        reference operator*() const { return *current_; }

        /// @brief Member access operator
        /// @return Pointer to current element
        pointer operator->() const { return current_; }

        /// @brief Pre-increment (moves backward in list)
        /// @return Reference to this iterator
        ReverseIterator& operator++() { current_ = current_->prev; return *this; }

        /// @brief Post-increment
        /// @return Copy of iterator before increment
        ReverseIterator operator++(int) { ReverseIterator tmp = *this; ++(*this); return tmp; }

        /// @brief Pre-decrement (moves forward in list)
        /// @return Reference to this iterator
        ReverseIterator& operator--() { current_ = current_->next; return *this; }

        /// @brief Post-decrement
        /// @return Copy of iterator before decrement
        ReverseIterator operator--(int) { ReverseIterator tmp = *this; --(*this); return tmp; }

        /// @brief Equality comparison
        bool operator==(const ReverseIterator& other) const { return current_ == other.current_; }

        /// @brief Inequality comparison
        bool operator!=(const ReverseIterator& other) const { return !(*this == other); }

    private:
        T* current_;
    };

    /// @brief Get reverse iterator to last element
    /// @return Reverse iterator pointing to last element
    ReverseIterator rbegin() { return ReverseIterator(tail_); }

    /// @brief Get reverse iterator to end (before first)
    /// @return Reverse iterator representing end
    ReverseIterator rend() { return ReverseIterator(nullptr); }

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /// @brief Check if element is in the list
    /// @param[in] item Element to check
    /// @return true if element is in the list
    /// @complexity O(n)
    bool contains(const T& item) const
    {
        T* current = head_;
        while (current)
        {
            if (current == &item) return true;
            current = current->next;
        }
        return false;
    }

    /// @brief Swap contents with another list
    /// @param[in,out] other List to swap with
    void swap(IntrusiveList& other) noexcept
    {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        std::swap(size_, other.size_);
    }

private:
    T* head_ = nullptr;   ///< Pointer to first element
    T* tail_ = nullptr;   ///< Pointer to last element
    size_t size_ = 0;     ///< Number of elements in list
};

} // namespace casket