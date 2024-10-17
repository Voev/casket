#include <sqlite3.h>

#include <casket/utils/string.hpp>

#include "sqlite3.hpp"

#include <string>
#ifdef _MSC_VER
#include <windows.h>
#define usleep(microsec) ::Sleep((microsec) / 1000)
#else
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

namespace casket::storage
{

class SQLite3::Result : public Backend::Result
{
public:
    Records recs;
    Record flds;
    Result()
    {
    }
    virtual size_t fieldNum() const;
    virtual Record fields() const;
    virtual size_t recordNum() const;
    virtual Records records() const;
    const Records& recordsRef() const;
};

size_t SQLite3::Result::fieldNum() const
{
    return flds.size();
}
Record SQLite3::Result::fields() const
{
    return flds;
}

size_t SQLite3::Result::recordNum() const
{
    return recs.size();
}
Records SQLite3::Result::records() const
{
    return recs;
}

/** SQLite3 - cursor */
class SQLite3::Cursor : public Backend::Cursor
{
    //        sqlite3 * db;
    sqlite3_stmt* stmt;
    const SQLite3& owner;

public:
    Cursor(/*sqlite3 * db,*/ sqlite3_stmt* s, const SQLite3& owner);
    virtual Record fetchOne();
    virtual ~Cursor();
};

SQLite3::Cursor::Cursor(sqlite3_stmt* s, const SQLite3& o)
    : stmt(s)
    , owner(o)
{
}

Record SQLite3::Cursor::fetchOne()
{
    bool busy = false;
    do
    {
        int status = sqlite3_step(stmt);

        switch (status)
        {
        case SQLITE_ERROR:
        case SQLITE_MISUSE:
        {
            std::string error = sqlite3_errmsg(owner.db_);
            throw std::runtime_error("step failed: " + toString(status) + error);
        }
        case SQLITE_DONE:
            return Record();
            break;
        case SQLITE_BUSY:
        case SQLITE_LOCKED:
            usleep(5000);
            busy = true;
            break;
        case SQLITE_ROW:
            busy = false;
            break;
        }
    } while (busy);
    int columnNum = sqlite3_data_count(stmt);
    Record rec(columnNum);
    for (int i = 0; i < columnNum; i++)
    {
        if (sqlite3_column_type(stmt, i) == SQLITE_NULL)
            rec.push_back("NULL");
        else
            rec.push_back((char*)sqlite3_column_text(stmt, i));
    }
    return rec;
}
SQLite3::Cursor::~Cursor()
{
    if (stmt)
    {
        sqlite3_finalize(stmt);
    }
}

SQLite3::SQLite3(const std::string& connInfo)
    : db_(nullptr)
    , transaction_(false)
    , beginTrans_("BEGIN")
{
    std::vector<std::string> params = utils::split(connInfo, ";");
    std::string database;
    for (size_t i = 0; i < params.size(); i++)
    {
        std::vector<std::string> param = utils::split(params[i], "=");
        if (param.size() == 1)
            continue;
        if (param[0] == "database")
            database = param[1];
        else if (param[0] == "transaction")
        {
            if (param[1] == "immediate")
                beginTrans_ = "BEGIN IMMEDIATE";
            else if (param[1] == "exclusive")
                beginTrans_ = "BEGIN EXCLUSIVE";
        }
    }
    if (database.empty())
    {
        throw std::runtime_error("no database-param specified");
    }
    else if (sqlite3_open(database.c_str(), &db_))
    {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
}

SQLite3::~SQLite3() noexcept
{
    if (db_)
    {
        sqlite3_close(db_);
    }
}

bool SQLite3::supportsSequences() const
{
    return false;
}

std::string SQLite3::getInsertID() const
{
    return toString(sqlite3_last_insert_rowid(db_));
}

void SQLite3::begin() const
{
    if (!transaction_)
    {
        execute(beginTrans_);
        transaction_ = true;
    }
}
void SQLite3::commit() const
{
    if (transaction_)
    {
        execute("COMMIT");
        transaction_ = false;
    }
}
void SQLite3::rollback() const
{
    if (transaction_)
    {
        execute("ROLLBACK");
        transaction_ = false;
    }
}

static int callback(void* r, int argc, char** argv, char** azColName)
{
    SQLite3::Result* res = (SQLite3::Result*)r;
    if (res->flds.empty())
        for (int i = 0; i < argc; i++)
            res->flds.push_back(azColName[i]);
    Record rec(argc);
    for (int i = 0; i < argc; i++)
        rec.push_back(argv[i] ? argv[i] : "NULL");
    res->recs.push_back(rec);
    return 0;
}

void SQLite3::throwError(int status) const
{
    std::string error = sqlite3_errmsg(db_);
    error = toString(status) + "=status code : " + error;
    switch (status)
    {
    case SQLITE_ERROR:
        throw std::runtime_error(error);
    case SQLITE_INTERNAL:
        throw std::runtime_error(error);
    case SQLITE_NOMEM:
        throw std::runtime_error(error);
    case SQLITE_FULL:
        throw std::runtime_error(error);
    case SQLITE_CONSTRAINT:
        throw std::runtime_error(error);
    default:
        throw std::runtime_error("compile failed: " + error);
    }
}

std::unique_ptr<Backend::Result> SQLite3::execute(const std::string& query) const
{
    std::string q(query);
    q.append(";");
    auto r = std::make_unique<Result>();
    char* errMsg;
    int status;
    do
    {
        status = sqlite3_exec(db_, q.c_str(), callback, r.get(), &errMsg);
        switch (status)
        {
        case SQLITE_BUSY:
        case SQLITE_LOCKED:
            usleep(250000);
            break;
        case SQLITE_OK:
            break;
        default:
            // fix for bug #3531292 (free resources before throwing error)
            sqlite3_free(errMsg);
            // end: fix for bug #3531292 (free resources before throwing error)

            throwError(status);
        }
    } while (status != SQLITE_OK);
    return r;
}

std::unique_ptr<Backend::Cursor> SQLite3::cursor(const std::string& query) const
{
    while (true)
    {
        sqlite3_stmt* stmt = NULL;
        int status =
            sqlite3_prepare(db_, query.c_str(), query.size(), &stmt, NULL);
        if (status != SQLITE_OK || stmt == NULL)
        {
            switch (status)
            {
            case SQLITE_BUSY:
            case SQLITE_LOCKED:
                usleep(250000);
                break;
            default:
                throwError(status);
            }
        }
        else
            return std::make_unique<Cursor>(stmt, *this);
    }
}

} // namespace casket::storage