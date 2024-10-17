#pragma once
#include <casket/storage/db.hpp>
#include <casket/storage/query_select.hpp>

#include <casket/utils/to_number.hpp>

namespace casket::storage {

/** returns QuerySelect which selects objects of type T
 * \param fdatas fields of class T
 * \param e optional filter expression
 */
QuerySelect selectObjectQuery(const std::vector<FieldType>& fdatas, const Expr& e = Expr());

/** returns QuerySelect which selects objects of type T
 * \param e optional filter expression
 */
template <class T>
QuerySelect selectObjectQuery(const Expr& e = Expr()) {
    std::vector<FieldType> fdatas;
    T::getFieldTypes(fdatas);
    return selectObjectQuery(fdatas, e);
}

/** template class which holds QuerySelect for selecting objects of type T */
template <class T>
class DataSource {
private:
    /** database reference, used to create cursors */
    const Database& db;
    /** selection query */
    QuerySelect sel;

public:
    /** \param db_ database reference
        \param e selection filter */
    DataSource(const Database& db_, const Expr& e = Expr())
        : db(db_)
        , sel(selectObjectQuery<T>(e)) {
    }
    /** \param db_ database reference
        \param sel_ selection query */
    DataSource(const Database& db_, const QuerySelect& sel_)
        : db(db_)
        , sel(sel_) {
    }

    /** returns database reference */
    const Database& getDatabase() const {
        return db;
    }
    /** returns QuerySelect which selects ID-numbers of objects */
    QuerySelect idQuery() const {
        QuerySelect idq(sel);
        idq.clearResults();
        idq.result(T::Id.fullName());
        return idq;
    }
    /** returns number of objects in result set */
    size_t count() const {
        QuerySelect cq(sel);
        cq.clearResults();
        cq.limit(0).offset(0);
        cq.result("count(*)");
        return utils::toNumber<size_t>(db.query(cq)[0][0]);
    }
    /** returns QuerySelect which selects objects */
    QuerySelect objectQuery() const {
        return sel;
    }
    /** returns cursor for query */
    Cursor<T> cursor() const {
        return db.template cursor<T>(sel);
    }
    /** returns first object in result set. throw exception if none found
     \return object of type T */
    T one() const {
        return *cursor();
    }
    /** returns all objects in result set. */
    std::vector<T> all() const {
        // \TODO a cursor is not appropriate here, because we fetch all results of the query
        return cursor().dump();
    }
    /** modifies QuerySelect to order result set
        \param f field to order by
        \param asc ascending order
        \return *this, methods can be chained */
    DataSource& orderBy(FieldType f, bool asc = true) {
        sel.orderBy(f.fullName(), asc);
        return *this;
    }
    /** modifies QuerySelect to order result set by external table
        \param id foreign key field used to join table with query
        \param f field to order by
        \param asc ascending order
        \return *this, methods can be chained */
    DataSource& orderByRelation(FieldType id, FieldType f, bool asc = true) {
        sel.source(id.table());
        sel.where(id == T::Id);
        sel.orderBy(f.fullName(), asc);
        return *this;
    }
};

} // namespace casket::storage
