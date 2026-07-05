#pragma once
#include <unordered_map>
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <fstream>

#include <casket/db/i_database.hpp>
#include <casket/db/txt/txt_row.hpp>
#include <casket/db/typed_field_value.hpp>

namespace casket::db
{

class TxtStatement;

class TxtDatabase : public IDatabase
{
private:
    using IndexMap = std::unordered_map<std::shared_ptr<IFieldValue>, size_t, FieldValueHash, FieldValueEqual>;

    using SortedIndexMap = std::map<std::shared_ptr<IFieldValue>, size_t, FieldValueCompare>;

    struct IndexInfo
    {
        std::unique_ptr<IndexMap> hashIndex;
        std::unique_ptr<SortedIndexMap> sortedIndex;
        std::function<bool(const IRow&)> qualifier;
        std::type_index fieldType;
        bool isSorted;

        IndexInfo();
        explicit IndexInfo(std::type_index type, bool sorted = false);
    };

    std::vector<std::type_index> fieldTypes_;
    std::vector<TxtRow> data_;
    std::vector<IndexInfo> indices_;
    mutable std::string lastError_;
    mutable int errorField_;
    mutable size_t errorRow_;
    mutable std::shared_ptr<IRow> errorRowData_;

    std::shared_ptr<IFieldValue> parseField(const std::string& str, std::type_index type);
    std::string escapeString(const std::string& str) const;
    std::string unescapeString(const std::string& str) const;
    void updateIndicesForRow(const TxtRow& row, size_t index, bool add);
    void rebuildIndices();
    bool validateRow(const TxtRow& row) const;
    bool checkIndexClashes(const TxtRow& row, size_t excludeIndex = SIZE_MAX) const;
    bool insert(TxtRow&& row);

public:
    explicit TxtDatabase(int fields);
    explicit TxtDatabase(const std::vector<std::type_index>& types);

    TxtDatabase(const TxtDatabase&) = delete;
    TxtDatabase& operator=(const TxtDatabase&) = delete;
    TxtDatabase(TxtDatabase&&) = default;
    TxtDatabase& operator=(TxtDatabase&&) = default;
    ~TxtDatabase() override = default;

    int getNumFields() const override;
    std::type_index getFieldType(int field) const override;
    void setFieldType(int field, std::type_index type) override;
    std::vector<std::type_index> getFieldTypes() const override;

    template <typename T>
    T getField(const IRow& row, int field) const
    {
        auto fieldValue = row.getField(field);
        if (!fieldValue || fieldValue->isNull())
        {
            throw Exception(ErrorType::INVALID_CONVERSION, "Field is null or empty");
        }

        auto typed = std::dynamic_pointer_cast<TypedFieldValue<T>>(fieldValue);
        if (typed)
        {
            return typed->getValue();
        }

        if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
        {
            auto timeField = std::dynamic_pointer_cast<TimePointField>(fieldValue);
            if (timeField)
            {
                return timeField->getValue();
            }
        }

        if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
        {
            auto binaryField = std::dynamic_pointer_cast<BinaryDataField>(fieldValue);
            if (binaryField)
            {
                return binaryField->getValue();
            }
        }

        throw Exception(ErrorType::TYPE_MISMATCH, "Cannot convert field to requested type");
    }

    size_t size() const override;
    const IRow& getRow(size_t index) const override;

    bool insert(std::shared_ptr<IRow> row) override;
    bool insert(const IRow& row) override;
    bool insert(IRow&& row) override;

    template <typename... Args>
    bool insert(Args... args)
    {
        TxtRow row(fieldTypes_);
        size_t idx = 0;
        ((row.setField(idx++, makeFieldValue(args))), ...);
        return insert(std::move(row));
    }

    bool update(size_t index, const IRow& newRow) override;
    bool remove(size_t index) override;
    void clear() override;

    bool createIndex(int field, std::function<bool(const IRow&)> qualifier = nullptr) override;
    bool createSortedIndex(int field, std::function<bool(const IRow&)> qualifier = nullptr) override;
    bool dropIndex(int field) override;
    bool hasIndex(int field) const override;
    bool hasSortedIndex(int field) const override;

    const IRow* findByIndex(int field, std::shared_ptr<IFieldValue> value) const override;
    std::vector<const IRow*> findRange(int field, std::shared_ptr<IFieldValue> start,
                                       std::shared_ptr<IFieldValue> end) const override;

    std::unique_ptr<IStatement> createStatement() override;

    void write(std::ostream& out) const override;
    void writeToFile(const std::string& filename) const override;
    void read(std::istream& in) override;
    void readFromFile(const std::string& filename) override;

    std::string getLastError() const override;
    int getErrorField() const override;
    size_t getErrorRow() const override;
    std::shared_ptr<IRow> getErrorRowData() const override;

    void print(std::ostream& out) const;
    bool isEmpty() const;
    void reserve(size_t capacity);
    void shrinkToFit();
};

} // namespace casket::db