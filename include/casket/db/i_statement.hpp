#pragma once
#include <chrono>
#include <vector>

#include <casket/nonstd/optional.hpp>
#include <casket/nonstd/string_view.hpp>
#include <casket/nonstd/span.hpp>

#include <casket/db/i_field_value.hpp>
#include <casket/db/i_row.hpp>

namespace casket::db
{

class IStatement
{
public:
    virtual ~IStatement() = default;

    virtual void bind(int column, nonstd::string_view str) = 0;
    virtual void bind(int column, size_t i) = 0;
    virtual void bind(int column, std::chrono::system_clock::time_point time) = 0;
    virtual void bind(int column, const std::vector<uint8_t>& blob) = 0;
    virtual void bind(int column, const uint8_t* data, size_t len) = 0;
    virtual void bindNull(int column) = 0;

    virtual void whereEquals(int column, std::shared_ptr<IFieldValue> value) = 0;
    virtual void whereBetween(int column, std::shared_ptr<IFieldValue> start, std::shared_ptr<IFieldValue> end) = 0;
    virtual void where(std::function<bool(const IRow&)> predicate) = 0;

    virtual void orderBy(int column, bool ascending = true) = 0;

    virtual void limit(size_t offset, size_t count) = 0;
    virtual void limit(size_t count) = 0;

    virtual size_t spin() = 0;
    virtual bool step() = 0;
    virtual const IRow* current() const = 0;

    virtual nonstd::span<const uint8_t> getBlob(int column) = 0;
    virtual nonstd::optional<std::string> getString(int column) = 0;
    virtual size_t getSizeType(int column) = 0;

    virtual void reset() = 0;
    virtual void clear() = 0;

    virtual size_t count() = 0;
    virtual double avg(int column) = 0;
    virtual double sum(int column) = 0;
    virtual double min(int column) = 0;
    virtual double max(int column) = 0;

    virtual std::vector<const IRow*> getResults() const = 0;
};

} // namespace casket::db