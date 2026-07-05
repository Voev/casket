#include <casket/db/txt/txt_row.hpp>

namespace casket::db
{

TxtRow::TxtRow(const std::vector<std::type_index>& types)
    : fieldTypes_(types)
{
    fields_.resize(types.size());
}

TxtRow::TxtRow(std::vector<std::shared_ptr<IFieldValue>>&& flds, const std::vector<std::type_index>& types)
    : fields_(std::move(flds))
    , fieldTypes_(types)
{
}

size_t TxtRow::size() const
{
    return fields_.size();
}

std::shared_ptr<IFieldValue> TxtRow::getField(size_t index) const
{
    if (index < fields_.size())
    {
        return fields_[index];
    }
    return nullptr;
}

void TxtRow::setField(size_t index, std::shared_ptr<IFieldValue> value)
{
    if (index < fields_.size())
    {
        fields_[index] = value;
    }
}

std::type_index TxtRow::getFieldType(size_t index) const
{
    if (index >= fieldTypes_.size())
    {
        return typeid(void);
    }
    return fieldTypes_[index];
}

std::string TxtRow::serialize() const
{
    std::string result;
    for (size_t i = 0; i < fields_.size(); ++i)
    {
        if (i > 0)
        {
            result += '\t';
        }
        if (fields_[i])
        {
            result += fields_[i]->toString();
        }
    }
    return result;
}

std::shared_ptr<IRow> TxtRow::clone() const
{
    auto newRow = std::make_shared<TxtRow>(fieldTypes_);
    for (size_t i = 0; i < fields_.size(); ++i)
    {
        if (fields_[i])
        {
            newRow->setField(i, fields_[i]->clone());
        }
    }
    return newRow;
}

} // namespace casket::db