#include <casket/storage/query_update.hpp>
#include <casket/utils/string.hpp>

namespace casket::storage
{

QueryUpdate::QueryUpdate(std::string table)
    : table_(std::move(table))
    , where_("True")
{
}

QueryUpdate::~QueryUpdate() noexcept
{
}

QueryUpdate& QueryUpdate::where(const Expr& e)
{
    where_ = (RawExpr(where_) && e).asString();
    return *this;
}

QueryUpdate& QueryUpdate::set(const FieldType& f, const std::string& value)
{
    fields.push_back(f.name());
    values.push_back(escapeSQL(value));
    return *this;
}

std::string QueryUpdate::toString() const
{
    std::string q = "UPDATE " + table_ + " SET ";
    std::vector<std::string> sets;
    for (size_t i = 0; i < fields.size(); i++)
        sets.push_back(fields[i] + "=" + values[i]);
    q += utils::join(sets, ",");
    if (!where_.empty())
        q += " WHERE " + where_;
    return q;
}

} // namespace casket::storage
