#pragma once
#include <functional>
#include <casket/db/i_statement.hpp>

namespace casket::db
{

class IDatabase
{
public:
    virtual ~IDatabase() = default;

    virtual int getNumFields() const = 0;
    virtual std::type_index getFieldType(int field) const = 0;
    virtual void setFieldType(int field, std::type_index type) = 0;
    virtual std::vector<std::type_index> getFieldTypes() const = 0;

    virtual size_t size() const = 0;
    virtual const IRow& getRow(size_t index) const = 0;

    virtual bool insert(std::shared_ptr<IRow> row) = 0;
    virtual bool insert(const IRow& row) = 0;
    virtual bool insert(IRow&& row) = 0;
    
    virtual bool update(size_t index, const IRow& newRow) = 0;
    virtual bool remove(size_t index) = 0;
    virtual void clear() = 0;

    virtual bool createIndex(int field, std::function<bool(const IRow&)> qualifier = nullptr) = 0;
    virtual bool createSortedIndex(int field, std::function<bool(const IRow&)> qualifier = nullptr) = 0;
    virtual bool dropIndex(int field) = 0;
    virtual bool hasIndex(int field) const = 0;
    virtual bool hasSortedIndex(int field) const = 0;

    virtual const IRow* findByIndex(int field, std::shared_ptr<IFieldValue> value) const = 0;
    virtual std::vector<const IRow*> findRange(int field, std::shared_ptr<IFieldValue> start,
                                               std::shared_ptr<IFieldValue> end) const = 0;

    virtual std::unique_ptr<IStatement> createStatement() = 0;

    virtual std::vector<const IRow*> query(std::function<void(IStatement&)> setup)
    {
        auto stmt = createStatement();
        setup(*stmt);
        stmt->spin();

        std::vector<const IRow*> result;
        while (stmt->step())
        {
            result.push_back(stmt->current());
        }
        return result;
    }

    virtual void write(std::ostream& out) const = 0;
    virtual void writeToFile(const std::string& filename) const = 0;
    virtual void read(std::istream& in) = 0;
    virtual void readFromFile(const std::string& filename) = 0;

    virtual std::string getLastError() const = 0;
    virtual int getErrorField() const = 0;
    virtual size_t getErrorRow() const = 0;
    virtual std::shared_ptr<IRow> getErrorRowData() const = 0;
};

} // namespace casket::db