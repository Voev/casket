#include <set>
#include <casket/storage/datasource.hpp>
#include <casket/storage/string.hpp>

#include <casket/utils/string.hpp>

namespace casket::storage
{

QuerySelect selectObjectQuery(const std::vector<FieldType>& fdatas,
                              const Expr& e)
{
    QuerySelect sel;
    std::vector<std::string> tables;
    std::set<std::string> tableSet;

    for (size_t i = 0; i < fdatas.size(); i++)
        if (tableSet.find(fdatas[i].table()) == tableSet.end())
        {
            tables.push_back(fdatas[i].table());
            tableSet.insert(fdatas[i].table());
        }

    std::vector<std::string> tableFilters;
    tableFilters.resize(tables.size() - 1);
    for (size_t i = 1; i < tables.size(); i++)
        tableFilters[i - 1] = tables[i - 1] + ".id_ = " + tables[i] + ".id_";
    tableSet.clear();
    for (size_t i = 0; i < tables.size(); i++)
    {
        sel.source(tables[i]);
        tableSet.insert(tables[i]);
    }
    if (tables.size() > 1)
        sel.where((e && RawExpr(utils::join(tableFilters, " AND "))).asString());
    else
        sel.where(e.asString());

    for (size_t i = 0; i < fdatas.size(); i++)
        sel.result(fdatas[i].table() + "." + fdatas[i].name());

    return sel;
}
} // namespace casket::storage
