#pragma once
#include <memory>

#include <casket/storage/record.hpp>
#include <casket/storage/commontypes.h>

namespace casket::storage {

/** An abstract base class for interfacing with relational databases */

/// @brief Абстрактный класс для взаимодействия с SQL БД.
class Backend {
public:
    /** An abstract base class for cursors that iterate result sets
      returned by relational database */
    class Cursor {
    public:
        /** empty */
        virtual ~Cursor() {
        }
        /** if inherited Cursor can cache its data to speed up
          iteration, this method is used to set cache size.
          All backends do not react to this request.
          */
        virtual void setCacheSize(int /*s*/) {
        }
        /** returns one result row. empty row means that result set is
         *  iterated through */
        virtual Record fetchOne() = 0;
    };

    class Result {
    public:
        /** empty */
        virtual ~Result() {
        }
        /** returns number of columns (fields) in result set */
        virtual size_t fieldNum() const = 0;
        /** returns names of columns (fields) of the result set */
        virtual Record fields() const = 0;
        /** returns number of rows (records) in result set */
        virtual size_t recordNum() const = 0;
        /** returns result set */
        virtual Records records() const = 0;
    };

    virtual ~Backend() = default;

    /** return true if backend supports CREATE SEQUENCE -
      SQL-statements */
    virtual bool supportsSequences() const = 0;

    virtual std::string getSQLType(AT_field_type fieldType, const std::string& length = "") const;
    virtual std::string getCreateSequenceSQL(const std::string& name) const;
    virtual std::string getSeqSQL(const std::string& sname) const;

    /** backend may want to set an AUTO_INCREMENT-attribute for table's primary
      key field. this method is to deliver the details to database schema */
    virtual std::string getRowIDType() const {
        return "INTEGER PRIMARY KEY";
    }

    /** if backend supports this, new primary key of the last insert
      is returned */
    virtual std::string getInsertID() const = 0;

    /** begin SQL transaction, may or may not have an effect */
    virtual void begin() const = 0;

    /** commit SQL transaction */
    virtual void commit() const = 0;

    /** rollback SQL transaction */
    virtual void rollback() const = 0;

    /** executes SQL-query
      \param query SQL-query to execute
      \return Result-object which holds result set of query */
    virtual std::unique_ptr<Result> execute(const std::string& query) const = 0;

    /** executes SQL-query
      \param query SQL-query to execute
      \return Cursor-object which can be used to iterate result set
      row by row without loading everything to memory */
    virtual std::unique_ptr<Cursor> cursor(const std::string& query) const = 0;
    
    /** executes multiple INSERT-statements and assigns same 'row id'
      for first field of every record
      \param tables destination tables for insert operation
      \param fields record of field names per table
      \param values record of values per table
      \param sequence sequence where row id-numbers are pulled
      \return new row id */
    virtual std::string
    groupInsert(const Record& tables, const Records& fields, const Records& values, const std::string& sequence) const;
    /** returns a backend according to Backendtype in type, parameters are specific to backend and are separated by
      semicolon. \param type type of the database backend (supported are : "mysql","postgresql","sqlite3","odbc" \param
      connInfo database connection specific parameters (parameters are separated by semicolon)
       @throw DatabaseError if no backend is found
      */
    static std::unique_ptr<Backend> getBackend(const std::string& type, const std::string& connInfo);
};

} // namespace casket::storage
