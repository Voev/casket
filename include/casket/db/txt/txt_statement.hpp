#pragma once
#include <casket/db/i_statement.hpp>
#include <casket/db/i_field_value.hpp>
#include <casket/db/i_row.hpp>
#include <casket/db/txt/txt_database.hpp>
#include <functional>
#include <vector>
#include <memory>

namespace casket::db
{

class TxtStatement : public IStatement
{
private:
    TxtDatabase* db_;

    struct Filter
    {
        enum Type
        {
            EQUALS,
            BETWEEN,
            CUSTOM,
            LIKE,
            IN,
            IS_NULL,
            IS_NOT_NULL,
            GREATER,
            LESS,
            GREATER_OR_EQUAL,
            LESS_OR_EQUAL
        };

        Type type;
        int column;
        std::shared_ptr<IFieldValue> value;
        std::shared_ptr<IFieldValue> start;
        std::shared_ptr<IFieldValue> end;
        std::vector<std::shared_ptr<IFieldValue>> values; // For IN operator
        std::function<bool(const IRow&)> predicate;
        std::string pattern; // For LIKE operator
        bool caseSensitive;  // For LIKE operator
    };

    std::vector<Filter> filters_;
    std::vector<std::pair<int, bool>> orderByClauses_;
    size_t limitOffset_;
    size_t limitCount_;
    bool hasLimit_;
    bool isExecuted_;
    bool isDistinct_;
    std::vector<int> distinctColumns_;

    std::vector<const IRow*> results_;
    std::vector<const IRow*>::iterator resultIterator_;

    bool matchesFilters(const IRow& row) const;
    bool matchesFilter(const IRow& row, const Filter& filter) const;
    bool matchesLike(const std::string& value, const std::string& pattern, bool caseSensitive) const;
    void applyDistinct();
    void applyOrderBy();
    void applyLimit();

public:
    explicit TxtStatement(TxtDatabase* database);
    ~TxtStatement() override = default;

    // Bind methods
    void bind(int column, std::string_view str) override;
    void bind(int column, size_t i) override;
    void bind(int column, std::chrono::system_clock::time_point time) override;
    void bind(int column, const std::vector<uint8_t>& blob) override;
    void bind(int column, const uint8_t* data, size_t len) override;
    void bindNull(int column) override;

    // Where conditions
    void whereEquals(int column, std::shared_ptr<IFieldValue> value) override;
    void whereBetween(int column, std::shared_ptr<IFieldValue> start, 
                      std::shared_ptr<IFieldValue> end) override;
    void where(std::function<bool(const IRow&)> predicate) override;
    
    // Additional where conditions
    void whereGreater(int column, std::shared_ptr<IFieldValue> value);
    void whereLess(int column, std::shared_ptr<IFieldValue> value);
    void whereGreaterOrEqual(int column, std::shared_ptr<IFieldValue> value);
    void whereLessOrEqual(int column, std::shared_ptr<IFieldValue> value);
    void whereLike(int column, const std::string& pattern, bool caseSensitive = false);
    void whereIn(int column, const std::vector<std::shared_ptr<IFieldValue>>& values);
    void whereIsNull(int column);
    void whereIsNotNull(int column);
    void whereNot(int column, std::shared_ptr<IFieldValue> value);

    // Order by
    void orderBy(int column, bool ascending = true) override;

    // Limit
    void limit(size_t offset, size_t count) override;
    void limit(size_t count) override;

    // Distinct
    void distinct();
    void distinct(int column);
    void distinct(const std::vector<int>& columns);

    // Execution
    size_t spin() override;
    bool step() override;
    const IRow* current() const override;

    // Get data from current row
    nonstd::span<const uint8_t> getBlob(int column) override;
    std::optional<std::string> getString(int column) override;
    size_t getSizeType(int column) override;

    // Reset and clear
    void reset() override;
    void clear() override;

    // Aggregate functions
    size_t count() override;
    double avg(int column) override;
    double sum(int column) override;
    double min(int column) override;
    double max(int column) override;

    // Get results
    std::vector<const IRow*> getResults() const override;

    // Additional utilities
    bool empty() const;
    size_t size() const;
    const IRow* first() const;
    const IRow* last() const;
};

} // namespace casket::db