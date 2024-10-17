#pragma once
#include <string>

#include <casket/storage/string.hpp>
#include <casket/storage/backend.hpp>

struct sqlite3;
struct sqlite3_stmt;

namespace casket::storage
{

class SQLite3 final : public Backend
{
public:
    class Cursor;
    class Result;

    explicit SQLite3(const std::string& database);
    ~SQLite3() noexcept;

    bool supportsSequences() const override;
    std::string getInsertID() const override;
    void begin() const override;
    void commit() const override;
    void rollback() const override;

    std::unique_ptr<Backend::Result> execute(const std::string& query) const;
    std::unique_ptr<Backend::Cursor> cursor(const std::string& query) const;

protected:
    void throwError(int status) const;

private:
    sqlite3* db_;
    mutable bool transaction_;
    std::string beginTrans_;
};
} // namespace casket::rom
