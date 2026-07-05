#pragma once
#include <vector>
#include <casket/db/i_row.hpp>

namespace casket::db
{

class TxtRow final : public IRow
{
public:
    TxtRow(const std::vector<std::type_index>& types);

    TxtRow(std::vector<std::shared_ptr<IFieldValue>>&& flds, const std::vector<std::type_index>& types);

    size_t size() const override;

    std::shared_ptr<IFieldValue> getField(size_t index) const override;

    void setField(size_t index, std::shared_ptr<IFieldValue> value) override;

    std::type_index getFieldType(size_t index) const override;

    std::string serialize() const override;

    std::shared_ptr<IRow> clone() const override;

private:
    std::vector<std::shared_ptr<IFieldValue>> fields_;
    std::vector<std::type_index> fieldTypes_;
};

} // namespace casket::db