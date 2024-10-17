#include <casket/storage/storage.hpp>

using namespace casket::storage;

int main()
{
    Database db("sqlite3", "database=test.db");
    return 0;
}