#pragma once
#include <vector>
#include <memory>
#include <casket/types/intrusive_list.hpp>

namespace casket
{

template <class T>
class Pool
{
private:
    using StorageType = std::vector<std::unique_ptr<T>>;
    using FreeListType = IntrusiveList<T>;

public:
    Pool(size_t poolSize)
    {
        storage_.reserve(poolSize);

        for (size_t i = 0; i < poolSize; ++i)
        {
            auto elem = std::make_unique<T>();
            freeList_.push_back(*elem);
            storage_.push_back(std::move(elem));
        }
    }

    T* acquire()
    {
        return freeList_.pop_front();
    }

    void release(T* p)
    {
        if (p)
        {
            freeList_.push_back(*p);
        }
    }

private:
    StorageType storage_;
    FreeListType freeList_;
};

}