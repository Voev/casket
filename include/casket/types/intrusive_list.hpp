#pragma once
#include <cstddef>
#include <cassert>
#include <memory>
#include <casket/utils/noncopyable.hpp>

namespace casket
{

/// Simplified double-linked intrusive list of std::shared_ptr.
/**
 * Const time insertion of std::shared_ptr.
 * Const time deletion of std::shared_ptr (deletion by value).
 *
 * Requirements:
 * if value is rvalue of type Value then expression
 * static_cast&lt;IntrusiveList&lt;Value&gt;::base_hook&amp;&gt;(value)
 * must be well formed and accessible from IntrusiveList&lt;Value&gt;.
 */
template <typename Value>
class IntrusiveList : private utils::NonCopyable
{
public:
    typedef Value value_type;
    typedef Value* pointer;
    typedef Value& reference;
    typedef std::weak_ptr<Value> weak_pointer;
    typedef std::shared_ptr<Value> shared_pointer;

    /// Required hook for items of the list.
    class base_hook;

    /// Never throws
    IntrusiveList();
    ~IntrusiveList();

    /// Never throws
    const shared_pointer& front() const;

    /// Never throws
    static shared_pointer prev(const shared_pointer& value);

    /// Never throws
    static const shared_pointer& next(const shared_pointer& value);

    /// Never throws
    void push_front(const shared_pointer& value);

    /// Never throws
    void erase(const shared_pointer& value);

    /// Never throws
    void clear();

    std::size_t size() const;

    bool empty() const;

private:
    static base_hook& get_hook(reference value);

    std::size_t size_;
    shared_pointer front_;
}; // class IntrusiveList

template <typename Value>
class IntrusiveList<Value>::base_hook
{
private:
    typedef base_hook this_type;

public:
    base_hook();
    base_hook(const this_type&);
    base_hook& operator=(const this_type&);

private:
    friend class IntrusiveList<Value>;
    weak_pointer prev_;
    shared_pointer next_;
}; // class IntrusiveList::base_hook

template <typename Value>
IntrusiveList<Value>::base_hook::base_hook()
{
}

template <typename Value>
IntrusiveList<Value>::base_hook::base_hook(const this_type&)
{
}

template <typename Value>
typename IntrusiveList<Value>::base_hook& IntrusiveList<Value>::base_hook::operator=(const this_type&)
{
    return *this;
}

template <typename Value>
IntrusiveList<Value>::IntrusiveList()
    : size_(0)
{
}

template <typename Value>
IntrusiveList<Value>::~IntrusiveList()
{
    clear();
}

template <typename Value>
const typename IntrusiveList<Value>::shared_pointer& IntrusiveList<Value>::front() const
{
    return front_;
}

template <typename Value>
typename IntrusiveList<Value>::shared_pointer IntrusiveList<Value>::prev(const shared_pointer& value)
{
    return get_hook(*value).prev_.lock();
}

template <typename Value>
const typename IntrusiveList<Value>::shared_pointer& IntrusiveList<Value>::next(const shared_pointer& value)
{
    return get_hook(*value).next_;
}

template <typename Value>
void IntrusiveList<Value>::push_front(const shared_pointer& value)
{
    BOOST_ASSERT_MSG(value, "The value can no not be null ptr");

    base_hook& value_hook = get_hook(*value);

    BOOST_ASSERT_MSG(!value_hook.prev_.lock() && !value_hook.next_, "The value to push has to be not linked");

    value_hook.next_ = front_;
    if (front_)
    {
        base_hook& front_hook = get_hook(*front_);
        front_hook.prev_ = value;
    }
    front_ = value;
    ++size_;
}

template <typename Value>
void IntrusiveList<Value>::erase(const shared_pointer& value)
{
    BOOST_ASSERT_MSG(value, "The value can no not be null ptr");

    base_hook& value_hook = get_hook(*value);
    if (value == front_)
    {
        front_ = value_hook.next_;
    }
    const shared_pointer prev = value_hook.prev_.lock();
    if (prev)
    {
        base_hook& prev_hook = get_hook(*prev);
        prev_hook.next_ = value_hook.next_;
    }
    if (value_hook.next_)
    {
        base_hook& next_hook = get_hook(*value_hook.next_);
        next_hook.prev_ = value_hook.prev_;
    }
    value_hook.prev_.reset();
    value_hook.next_.reset();
    --size_;

    BOOST_ASSERT_MSG(!value_hook.prev_.lock() && !value_hook.next_, "The erased value has to be unlinked");
}

template <typename Value>
void IntrusiveList<Value>::clear()
{
    // We don't want to have recusrive calls of wrapped_session's destructor
    // because the deep of such recursion may be equal to the size of list.
    // The last can be too great for the stack.
    while (front_)
    {
        base_hook& front_hook = get_hook(*front_);
        shared_pointer tmp;
        tmp.swap(front_hook.next_);
        front_hook.prev_.reset();
        front_.swap(tmp);
    }
    size_ = 0;
}

template <typename Value>
std::size_t IntrusiveList<Value>::size() const
{
    return size_;
}

template <typename Value>
bool IntrusiveList<Value>::empty() const
{
    return 0 == size_;
}

template <typename Value>
typename IntrusiveList<Value>::base_hook& IntrusiveList<Value>::get_hook(reference value)
{
    return static_cast<base_hook&>(value);
}

} // namespace casket

