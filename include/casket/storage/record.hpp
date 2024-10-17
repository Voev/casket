#pragma once
#include <vector>
#include <string>

namespace casket::storage {

class Database;

/** SQL data row wrapper. */
typedef std::vector<std::string> Record;

/** shortcut */
typedef std::vector<Record> Records;

} // namespace casket::storage
