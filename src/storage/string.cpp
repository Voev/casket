#include <casket/storage/string.hpp>
#include <casket/utils/string.hpp>

namespace casket::storage {

std::string escapeSQL(const std::string& str) {
    std::string tmp;
    if (str == "NULL")
        return "NULL";

    tmp = utils::replace(str, "'NULL'", "NULL");
    return "'" + utils::replace(tmp, "'", "''") + "'";
}

} // namespace casket::storage
