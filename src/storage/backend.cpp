
#include <casket/storage/backend.hpp>
#include <casket/storage/string.hpp>

#include <casket/utils/string.hpp>

#include "sqlite/sqlite3.hpp"

namespace casket::storage
{

std::string Backend::getSQLType(AT_field_type fieldType,
                                const std::string&) const
{
    switch (fieldType)
    {
    case A_field_type_integer:
        return "INTEGER";
    case A_field_type_bigint:
        return "BIGINT";
    case A_field_type_string:
        return "TEXT";
    case A_field_type_float:
        return "FLOAT";
    case A_field_type_double:
        return "DOUBLE";
    case A_field_type_boolean:
        return "INTEGER";
    case A_field_type_date:
        return "INTEGER";
    case A_field_type_time:
        return "INTEGER";
    case A_field_type_datetime:
        return "INTEGER";
    case A_field_type_blob:
        return "BLOB";
    default:
        return "";
    }
}

std::string Backend::getCreateSequenceSQL(const std::string& name) const
{
    return "CREATE SEQUENCE " + name + " START 1 INCREMENT 1";
}

std::string Backend::getSeqSQL(const std::string& sname) const
{
    std::string ret = "SELECT nextval('" + sname + "')";
    return ret;
}

std::string Backend::groupInsert(const Record& tables, const Records& fields,
                                 const Records& _values,
                                 const std::string& sequence) const
{
    std::string id = _values[0][0];
    Records insertValues = _values;
    if (supportsSequences() && id == "NULL")
    {
        auto r = execute(getSeqSQL(sequence));
        id = r->records()[0][0];
    }
    for (int i = tables.size() - 1; i >= 0; i--)
    {
        std::string fieldString = utils::join(fields[i], ",");
        std::string valueString;
        if (!insertValues[i].empty())
            insertValues[i][0] = id;
        std::vector<std::string> valueSplit = insertValues[i];
        for (size_t i2 = 0; i2 < valueSplit.size(); i2++)
            valueSplit[i2] = escapeSQL(valueSplit[i2]);
        valueString = utils::join(valueSplit, ",");
        std::string query = "INSERT INTO " + tables[i] + " (" + fieldString +
                            ") VALUES (" + valueString + ")";
        execute(query);
        if (!supportsSequences() && id == "NULL")
            id = getInsertID();
    }
    return id;
}

std::unique_ptr<Backend> Backend::getBackend(const std::string& backendType,
                                             const std::string& connInfo)
{
    Backend* backend = nullptr;
    if (backendType == "sqlite3")
    {
        backend = new SQLite3(connInfo);
    }
    return std::unique_ptr<Backend>(backend);
}

} // namespace casket::storage