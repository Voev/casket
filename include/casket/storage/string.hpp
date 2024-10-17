#pragma once
#include <string>
#include <sstream>
#include <vector>

namespace casket::storage {
/** returns string representation of passed parameter if it can be
 written to ostringstream */
template <class T>
std::string toString(T a) {
    std::ostringstream ost;
    ost << a;
    return ost.str();
}

/** escapes ' characters so that they do not break SQL statements.
 Note: 'NULL' is escaped to NULL */
std::string escapeSQL(const std::string& str);

} // namespace casket::storage
