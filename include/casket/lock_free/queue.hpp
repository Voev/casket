#pragma once
#include <optional>
#include <casket/lock_free/atomic_shared_ptr.hpp>

namespace casket::lock_free
{

template <typename T>
class Queue
{
    struct Node
    {
        Node() = default;

        AtomicSharedPtr<Node> next;
        T data;
        std::atomic_flag consumed = ATOMIC_FLAG_INIT;
    };

public:
    Queue();

    void push(const T& data);
    std::optional<T> pop();

private:
    AtomicSharedPtr<Node> front;
    AtomicSharedPtr<Node> back;
};

template <typename T>
Queue<T>::Queue()
{
    auto fakeNode = SharedPtr(new Node{});
    fakeNode->consumed.test_and_set();

    front.compareExchange(nullptr, fakeNode.copy());
    back.compareExchange(nullptr, std::move(fakeNode));
}

template <typename T>
void Queue<T>::push(const T& data)
{
    auto newBack = SharedPtr(new Node{});

    newBack->data = data;

    while (true)
    {
        FastSharedPtr<Node> currentBack = back.getFast();
        if (currentBack.get()->next.compareExchange(nullptr, newBack.copy()))
        {
            back.compareExchange(currentBack.get(), std::move(newBack));
            return;
        }
        else
        {
            SharedPtr<Node> realPtr = currentBack.get()->next.get();
            assert(realPtr.get() != nullptr);
            back.compareExchange(currentBack.get(), std::move(realPtr));
        }
    }
}

template <typename T>
std::optional<T> Queue<T>::pop()
{
    FastSharedPtr<Node> res = front.getFast();
    while (res.get()->consumed.test_and_set())
    {
        auto nextPtr = res.get()->next.get();
        if (nextPtr.get() == nullptr)
        {
            return {};
        }
        front.compareExchange(res.get(), nextPtr.copy());
        res = front.getFast();
    }

    return {res.get()->data};
}

} // namespace casket::lock_free