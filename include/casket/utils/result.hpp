#pragma once
#include <type_traits>
#include <utility>
#include <cassert>

namespace casket
{

template <typename T, typename E>
class Result
{
private:
    union Storage
    {
        T value;
        E error;

        Storage()
        {
        }
        ~Storage()
        {
        }
    };

    Storage storage_;
    bool has_value_;

    void construct_value(T&& value)
    {
        new (&storage_.value) T(std::move(value));
        has_value_ = true;
    }

    void construct_value(const T& value)
    {
        new (&storage_.value) T(value);
        has_value_ = true;
    }

    void construct_error(E&& error)
    {
        new (&storage_.error) E(std::move(error));
        has_value_ = false;
    }

    void construct_error(const E& error)
    {
        new (&storage_.error) E(error);
        has_value_ = false;
    }

    void destroy()
    {
        if (has_value_)
        {
            storage_.value.~T();
        }
        else
        {
            storage_.error.~E();
        }
    }

public:
    Result(const T& value)
    {
        construct_value(value);
    }

    Result(T&& value)
    {
        construct_value(std::move(value));
    }

    Result(const E& error)
    {
        construct_error(error);
    }

    Result(E&& error)
    {
        construct_error(std::move(error));
    }

    Result(const Result& other)
        : has_value_(other.has_value_)
    {
        if (has_value_)
        {
            construct_value(other.storage_.value);
        }
        else
        {
            construct_error(other.storage_.error);
        }
    }

    Result(Result&& other) noexcept
        : has_value_(other.has_value_)
    {
        if (has_value_)
        {
            construct_value(std::move(other.storage_.value));
        }
        else
        {
            construct_error(std::move(other.storage_.error));
        }
        other.has_value_ = false; // other теперь в "пустом" состоянии
    }

    ~Result()
    {
        destroy();
    }

    Result& operator=(const Result& other)
    {
        if (this != &other)
        {
            destroy();
            has_value_ = other.has_value_;
            if (has_value_)
            {
                construct_value(other.storage_.value);
            }
            else
            {
                construct_error(other.storage_.error);
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept
    {
        if (this != &other)
        {
            destroy();
            has_value_ = other.has_value_;
            if (has_value_)
            {
                construct_value(std::move(other.storage_.value));
            }
            else
            {
                construct_error(std::move(other.storage_.error));
            }
            other.has_value_ = false;
        }
        return *this;
    }

    bool has_value() const
    {
        return has_value_;
    }
    explicit operator bool() const
    {
        return has_value_;
    }

    T& value()
    {
        assert(has_value_);
        return storage_.value;
    }

    const T& value() const
    {
        assert(has_value_);
        return storage_.value;
    }

    T& operator*()
    {
        return value();
    }
    const T& operator*() const
    {
        return value();
    }

    T* operator->()
    {
        return &value();
    }
    const T* operator->() const
    {
        return &value();
    }

    E& error()
    {
        assert(!has_value_);
        return storage_.error;
    }

    const E& error() const
    {
        assert(!has_value_);
        return storage_.error;
    }

    T value_or(T&& default_value) const
    {
        return has_value_ ? storage_.value : std::move(default_value);
    }

    template <typename F>
    auto and_then(F&& f) const -> Result<typename std::invoke_result_t<F, T>, E>
    {
        if (has_value_)
        {
            return f(storage_.value);
        }
        return Result<typename std::invoke_result_t<F, T>, E>(storage_.error);
    }

    template <typename F>
    Result<T, E> or_else(F&& f) const
    {
        if (!has_value_)
        {
            f(storage_.error);
        }
        return *this;
    }
};

} // namespace casket