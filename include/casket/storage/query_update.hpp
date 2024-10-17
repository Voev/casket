#pragma once
#include <casket/storage/expr.hpp>

namespace casket::storage
{

class QueryUpdate
{
public:
    explicit QueryUpdate(std::string table);
    ~QueryUpdate() noexcept;

    QueryUpdate& where(const Expr& e);
    QueryUpdate& set(const FieldType& f, const std::string& value);
    std::string toString() const;

private:
    std::string table_;
    std::string where_;
    std::vector<std::string> fields;
    std::vector<std::string> values;
};

} // namespace casket::storage
