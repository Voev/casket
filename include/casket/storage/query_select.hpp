#pragma once
#include <casket/storage/expr.hpp>

namespace casket::storage {

class QuerySelect {
public:
    QuerySelect()
        : _distinct(false)
        , _limit(0)
        , _offset(0)
        , _where("True") {
    }
    QuerySelect& distinct(bool d);
    QuerySelect& limit(int value);
    QuerySelect& offset(int value);
    QuerySelect& result(const std::string& r);
    QuerySelect& clearResults();
    QuerySelect& source(const std::string& s, const std::string& alias = "");
    QuerySelect& where(const Expr& w);
    QuerySelect& where(const std::string& w);
    QuerySelect& groupBy(const std::string& gb);
    QuerySelect& having(const Expr& h);
    QuerySelect& having(const std::string& h);

    QuerySelect& orderBy(const std::string& ob, bool ascending = true);

    operator std::string() const;

    std::string asString() const {
        return this->operator std::string();
    }

private:
    bool _distinct;
    int _limit, _offset;
    std::vector<std::string> _results;
    std::vector<std::string> _sources;
    std::string _where;
    std::vector<std::string> _groupBy;
    std::string _having;
    std::vector<std::string> _orderBy;
};

} // namespace casket::storage
