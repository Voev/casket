#pragma once
#include <cstddef>
#include <string>
#include <memory>
#include <typeindex>

namespace casket::db
{

class IFieldValue
{
public:
    virtual ~IFieldValue() = default;

    virtual std::shared_ptr<IFieldValue> clone() const = 0;

    virtual bool equals(const IFieldValue& other) const = 0;

    virtual int compare(const IFieldValue& other) const = 0;

    virtual std::type_index getType() const = 0;

    virtual size_t hash() const = 0;

    virtual bool isNull() const = 0;

    virtual std::string toString() const = 0;
};

struct FieldValueHash
{
    size_t operator()(const std::shared_ptr<IFieldValue>& fv) const
    {
        return fv ? fv->hash() : 0;
    }
};

struct FieldValueEqual
{
    bool operator()(const std::shared_ptr<IFieldValue>& a, const std::shared_ptr<IFieldValue>& b) const
    {
        if (!a && !b)
            return true;
        if (!a || !b)
            return false;
        return a->equals(*b);
    }
};

struct FieldValueCompare
{
    bool operator()(const std::shared_ptr<IFieldValue>& a, const std::shared_ptr<IFieldValue>& b) const
    {
        if (!a && !b)
            return false;
        if (!a)
            return true;
        if (!b)
            return false;
        return a->compare(*b) < 0;
    }
};

} // namespace casket::db