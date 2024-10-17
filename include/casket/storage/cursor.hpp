#pragma once
#include <casket/storage/backend.hpp>
#include <casket/utils/exception.hpp>

namespace casket::storage
{

class Database;

template <class T>
class Cursor
{
public:
    Cursor(const Database& db, Backend::Cursor* c)
        : db_(db)
        , cursor_(c)
        , done_(false)
        , dataReady_(false)
    {
        operator++();
    }
    /** deletes Backend::Cursor */
    ~Cursor() noexcept
    {
        delete cursor_;
    }

    /** steps to next record */
    Cursor<T>& operator++()
    {
        if (done_)
            return *this;
        currentRow_ = cursor_->fetchOne();
        if (currentRow_.size() == 0)
        {
            done_ = true;
            dataReady_ = false;
        }
        else
            dataReady_ = true;

        return *this;
    }

    /** steps to next record */
    Cursor<T>& operator++(int)
    {
        return operator++();
    }
    /** returns the rest of the result set in vector */
    std::vector<T> dump()
    {
        std::vector<T> res;
        for (; !done_; operator++())
            res.push_back(operator*());
        return res;
    }
    /** returns current record */
    T operator*()
    {
        Record rec;
        utils::ThrowIfFalse(dataReady_, "data is not ready");
        return T(db_, currentRow_);
    }
    /** returns true if there are records left in the result set */
    inline bool rowsLeft() const
    {
        return !done_;
    }

private:
    /** Database - reference which is passed to created(T) - objects */
    const Database& db_;
    /** implementation of cursor */
    Backend::Cursor* cursor_;
    /** current record */
    Record currentRow_;
    /** the result set iterated is through */
    bool done_;
    /** got data to access */
    bool dataReady_;
};

} // namespace casket::storage
