#include <casket/storage/query_select.hpp>
#include <casket/utils/string.hpp>

using std::string;

using namespace casket::storage;

QuerySelect& QuerySelect::distinct(bool d) {
    _distinct = d;
    return *this;
}
QuerySelect& QuerySelect::limit(int value) {
    _limit = value;
    return *this;
}
QuerySelect& QuerySelect::offset(int value) {
    _offset = value;
    return *this;
}
QuerySelect& QuerySelect::result(const std::string& r) {
    _results.push_back(r);
    return *this;
}
QuerySelect& QuerySelect::clearResults() {
    _results.clear();
    return *this;
}
QuerySelect& QuerySelect::source(const std::string& s, const std::string& alias) {
    string a(s);
    if (!alias.empty())
        a += " AS " + alias;
    _sources.push_back(a);
    return *this;
}
QuerySelect& QuerySelect::where(const Expr& w) {
    _where = (RawExpr(_where) && w).asString();
    return *this;
}
QuerySelect& QuerySelect::where(const std::string& w) {
    _where = (RawExpr(_where) && RawExpr(w)).asString();
    return *this;
}
QuerySelect& QuerySelect::groupBy(const std::string& gb) {
    _groupBy.push_back(gb);
    return *this;
}
QuerySelect& QuerySelect::having(const Expr& h) {
    _having = h.asString();
    return *this;
}
QuerySelect& QuerySelect::having(const std::string& h) {
    _having = h;
    return *this;
}
QuerySelect& QuerySelect::orderBy(const std::string& ob, bool ascending) {
    std::string value = ob;
    if (!ascending)
        value += " DESC";
    _orderBy.push_back(value);
    return *this;
}
QuerySelect::operator string() const {
    std::string res = "SELECT ";
    if (_distinct)
        res += "DISTINCT ";
    res += utils::join(_results, ",");
    res += " FROM ";
    res += utils::join(_sources, ",");
    if (_where != "True")
        res += " WHERE " + _where;
    if (_groupBy.size() > 0)
        res += " GROUP BY " + utils::join(_groupBy, ",");
    if (!_having.empty()) 
        res += " HAVING " + _having;
    if (_orderBy.size() > 0)
        res += " ORDER BY " + utils::join(_orderBy, ",");
    if (_limit)
        res += " LIMIT " + toString(_limit);
    if (_offset)
        res += " OFFSET " + toString(_offset);
    return res;
}
