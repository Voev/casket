#pragma once

namespace casket::storage
{

/** base class of relation handles attached to Persistent objects */
template <class T>
class RelationHandle
{
protected:
    const T* owner;
    RelationHandle(const T& o)
        : owner(&o)
    {
    }
};

} // namespace casket::storage
