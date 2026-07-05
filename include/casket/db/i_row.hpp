#pragma once
#include <casket/db/i_field_value.hpp>

namespace casket::db
{

class IRow
{
public:
    virtual ~IRow() = default;

    virtual std::shared_ptr<IRow> clone() const = 0;

    virtual size_t size() const = 0;

    virtual void setField(size_t index, std::shared_ptr<IFieldValue> value) = 0;

    virtual std::type_index getFieldType(size_t index) const = 0;

    virtual std::shared_ptr<IFieldValue> getField(size_t index) const = 0;

    virtual std::string serialize() const = 0;
};

} // namespace casket::db