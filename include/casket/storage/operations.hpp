#pragma once
#include <casket/storage/datasource.hpp>

namespace casket::storage
{

/** returns DataSource for accessing objects of type T */
template <class T>
DataSource<T> select(const Database& db, const Expr& e = Expr())
{
    return DataSource<T>(db, e);
}
/** returns DataSource for accessing intersection of two sets of objects
 * of type T */
template <class T>
DataSource<T> intersect(const DataSource<T>& ds1, const DataSource<T>& ds2)
{
    auto sel =
        ds1.idQuery().asString() + " INTERSECT " + ds2.idQuery().asString();
    return DataSource<T>(ds1.getDatabase(), T::Id.in(sel));
}
/** returns DataSource for accessing union of two sets of objects of type T */
template <class T>
DataSource<T> union_(const DataSource<T>& ds1, const DataSource<T>& ds2)
{
    auto sel =
        ds1.idQuery().asString() + " UNION " + ds2.idQuery().asString();
    return DataSource<T>(ds1.getDatabase(), T::Id.in(sel));
}
/** returns DataSource for accessing objects of type T that are in first
 *  DataSource but not in second. */
template <class T>
DataSource<T> except(const DataSource<T>& ds1, const DataSource<T>& ds2)
{
    auto sel =
        ds1.idQuery().asString() + " EXCEPT " + ds2.idQuery().asString();
    return DataSource<T>(ds1.getDatabase(), T::Id.in(sel));
}

} // namespace casket::storage
