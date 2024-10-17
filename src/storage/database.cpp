#include <map>
#include <algorithm>
#include <casket/storage/db.hpp>
#include <casket/storage/cursor.hpp>
#include <casket/storage/query_select.hpp>

#include <casket/utils/string.hpp>

namespace casket::storage
{

typedef std::vector<ColumnDefinition> ColumnDefinitions;

void Database::openDatabase()
{
    backend_ = Backend::getBackend(backendType, connInfo);
}

void Database::storeSchemaItem(const SchemaItem& s) const
{
    delete_("schema_",
            RawExpr("name_='" + s.name + "' and type_='" + s.type + "'"));
    Record values(3);

    values.push_back(s.name);
    values.push_back(s.type);
    values.push_back(s.sql);
    insert("schema_", values);
}

std::vector<Database::SchemaItem> Database::getCurrentSchema() const
{
    QuerySelect sel;
    sel.result("name_").result("type_").result("sql_").source("schema_");
    std::vector<SchemaItem> s;

    Records recs;

    try
    {
        recs = query(sel);
    }
    catch (std::exception& e)
    {
        return std::vector<Database::SchemaItem>();
    }

    for (size_t i = 0; i < recs.size(); i++)
    {
        s.push_back(SchemaItem(recs[i][0], recs[i][1], recs[i][2]));
    }
    return s;
}

bool operator==(const ColumnDefinition& c1, const ColumnDefinition& c2)
{
    return (c1.name == c2.name) && (c1.type == c2.type);
}

static ColumnDefinitions getFields(const std::string& schema)
{
    ColumnDefinitions fields;
    int start = schema.find("(");
    int end = schema.find(")");
    if (start == -1 || end == -1)
        return fields;
    std::vector<std::string> tmp = utils::split(
        utils::replace(schema.substr(start + 1, end - start - 1), ", ", ","), ",");

    ColumnDefinition field_def;
    for (auto fdef : tmp)
    {
        std::vector<std::string> s = utils::split(fdef, " ");
        field_def.name = s[0];
        field_def.type = s[1];
        fields.push_back(field_def);
    }
    return fields;
}

bool Database::addColumn(const std::string& name,
                         const ColumnDefinition& column_def) const
{
    query("ALTER TABLE " + name + " ADD COLUMN " + column_def.name + " " +
          column_def.type);
    return true;
}

void Database::upgradeTable(const std::string& name, const std::string& oldSchema,
                            const std::string& newSchema) const
{
    ColumnDefinitions oldFields = getFields(oldSchema);
    ColumnDefinitions newFields = getFields(newSchema);

    ColumnDefinitions toAdd(newFields);
    ColumnDefinitions::iterator found;

    for (ColumnDefinitions::iterator it = oldFields.begin();
         it != oldFields.end(); it++)
    {
        found = find_if(toAdd.begin(), toAdd.end(),
                        ColumnDefinition::EqualName(it->name));
        if (found != toAdd.end())
        {
            toAdd.erase(found);
        }
    }

    ColumnDefinitions commonFields;
    for (ColumnDefinitions::iterator it = oldFields.begin();
         it != oldFields.end(); it++)
    {
        found = find_if(newFields.begin(), newFields.end(),
                        ColumnDefinition::EqualName(it->name));
        if (found != newFields.end())
        {
            commonFields.push_back(*found);
        }
    }

    begin();
    std::string bkp_name(name + "backup");
    query(" ALTER TABLE " + name + " RENAME TO " + bkp_name);
    for (ColumnDefinitions::iterator it = toAdd.begin(); it != toAdd.end();
         it++)
    {
        addColumn(bkp_name, *it);
    }

    query(newSchema);
    // oldfields as ...
    std::vector<std::string> cols;
    std::vector<std::string> colNames;
    std::string s;

    for (ColumnDefinitions::iterator it = commonFields.begin();
         it != commonFields.end(); it++)
    {
        s = it->name;
        s.append(" AS ");
        s.append(it->name);
        cols.push_back(s);
        colNames.push_back(it->name);
    }

    for (ColumnDefinitions::iterator it = toAdd.begin(); it != toAdd.end();
         it++)
    {
        s = it->name;
        s.append(" AS ");
        s.append(it->name);
        cols.push_back(s);
        colNames.push_back(it->name);
    }

    query(" INSERT INTO " + name + " (" + utils::join(colNames, ",") + ") SELECT " +
          utils::join(cols, ",") + " FROM " + bkp_name);
    query(" DROP TABLE " + bkp_name);
    commit();
}

Database::Database(const std::string& backend, const std::string& conn)
    : backendType(backend)
    , connInfo(conn)
    , backend_()
{
    openDatabase();
}

Database::Database(const Database& op)
    : backendType(op.backendType)
    , connInfo(op.connInfo)
{
    openDatabase();
}

Database::~Database()
{
}

void Database::create() const
{
    std::vector<SchemaItem> s = getSchema();
    begin();
    for (size_t i = 0; i < s.size(); i++)
    {
        query(s[i].sql);
        storeSchemaItem(s[i]);
    }
    commit();
}

void Database::drop() const
{
    std::vector<SchemaItem> s = getSchema();

    for (size_t i = 0; i < s.size(); i++)
    {
        try
        {
            begin();
            if (s[i].type == "table")
                query("DROP TABLE " + s[i].name);
            else if (s[i].type == "sequence")
                query("DROP SEQUENCE " + s[i].name);
            commit();
        }
        catch (std::exception& e)
        {
            (void)e;
            rollback();
        }
    }
}

bool Database::needsUpgrade() const
{
    std::vector<SchemaItem> cs = getCurrentSchema();
    std::vector<SchemaItem> s = getSchema();
    std::map<std::string, int> items;
    for (size_t i = 0; i < cs.size(); i++)
        items[cs[i].name] = i;

    for (size_t i = 0; i < s.size(); i++)
    {
        if (items.find(s[i].name) == items.end() ||
            cs[items[s[i].name]].sql != s[i].sql)
            return true;
    }
    return false;
}

void Database::upgrade() const
{
    std::vector<SchemaItem> cs = getCurrentSchema();
    std::vector<SchemaItem> s = getSchema();
    std::map<std::string, int> items;
    for (size_t i = 0; i < cs.size(); i++)
    {
        items[cs[i].name] = i;
    }
    begin();
    for (size_t i = 0; i < s.size(); i++)
    {
        if (items.find(s[i].name) == items.end())
        {
            query(s[i].sql);
            storeSchemaItem(s[i]);
            continue;
        }
        if (s[i].type == "table" && cs[items[s[i].name]].sql != s[i].sql)
        {
            upgradeTable(s[i].name, cs[items[s[i].name]].sql, s[i].sql);
            storeSchemaItem(s[i]);
        }
    }
    commit();
}
Records Database::query(const std::string& q) const
{
    std::unique_ptr<Backend::Result> r(backend_->execute(q));
    return r->records();
}

void Database::insert(const std::string& table, const Record& r,
                      const std::vector<std::string>& fields) const
{
    std::string command = "INSERT INTO " + table;
    if (fields.size())
        command += " (" + utils::join(fields, ",") + ")";
    command += " VALUES (";
    unsigned int i;
    for (i = 0; i < r.size() - 1; i++)
    {
        command += escapeSQL(r[i]) + ",";
    }
    command += escapeSQL(r[i]) + ")";
    query(command);
}

std::string Database::groupInsert(const Record& tables, const Records& fields,
                             const Records& values,
                             const std::string& sequence) const
{
    return backend_->groupInsert(tables, fields, values, sequence);
}

void Database::delete_(const std::string& table, const Expr& e) const
{
    std::string where = "";
    if (e.asString() != "True")
        where = " WHERE " + e.asString();
    query("DELETE FROM " + table + where);
}

} // namespace casket::storage