#pragma once
#include <atomic>
#include <cstddef>

namespace casket
{

template <typename T>
struct ListNode
{
    T elem;
    std::atomic<ListNode*> next{nullptr};

    static ListNode* node_from_elem(T* elem)
    {
        return reinterpret_cast<ListNode*>(reinterpret_cast<char*>(elem) - offsetof(ListNode, elem));
    }
};

} // namespace casket
