#include <iostream>
#include <casket/storage/persistent.hpp>
#include <casket/storage/query_update.hpp>

namespace casket::storage {
using std::string;

const Persistent& Persistent::operator=(const Persistent& p) {
    if (this != &p) {
        db = p.db;
        inDatabase = p.inDatabase;
        oldKey = p.oldKey;
    }
    return *this;
}

string Persistent::insert(Record& tables, Records& fieldRecs, Records& values, const string& sequence) {
    if (values[0][0] == "0")
        for (size_t i = 0; i < values.size(); i++)
            values[i][0] = "NULL";
    string key = db->groupInsert(tables, fieldRecs, values, sequence);
    oldKey = utils::toNumber<int>(key);
    inDatabase = true;
    return key;
}
void Persistent::update(const Updates& updates) {
    for (Updates::const_iterator i = updates.begin(); i != updates.end(); i++) {
        QueryUpdate uq(i->first);
        uq.where(RawExpr("id_ = '" + toString(oldKey) + "'"));
        bool notEmpty = false;
        for (auto i2 = i->second.begin(); i2 != i->second.end(); i2++) {
            uq.set(i2->first, i2->second);
            notEmpty = true;
        }
        if (notEmpty)
            db->query(uq.toString());
    }
}
void Persistent::prepareUpdate(Updates& updates, const string& table) {
    if (updates.find(table) == updates.end()) {
        updates[table] = std::vector<std::pair<FieldType, string>>();
    }
}
void Persistent::deleteFromTable(const string& table, const string& id) {
    db->query("DELETE FROM " + table + " WHERE id_=" + escapeSQL(id));
}
} // namespace casket::storage
