#pragma once
#include <stdexcept>

namespace casket::db
{

enum class ErrorType
{
    INVALID_ARGUMENT = 0,
    MALLOC_ERROR,
    WRONG_NUM_FIELDS,
    INDEX_OUT_OF_RANGE,
    NO_INDEX,
    INDEX_CLASH,
    TYPE_MISMATCH,
    INVALID_CONVERSION,
    IO_ERROR,s
};

class Exception : public std::exception
{
public:
    Exception(ErrorType type, const std::string& msg = "")
    {
        switch (type)
        {
        case ErrorType::INVALID_ARGUMENT:
            msg_ = "invalid argument";
            break;
        case ErrorType::MALLOC_ERROR:
            msg_ = "memory allocation error";
            break;
        case ErrorType::WRONG_NUM_FIELDS:
            msg_ = "wrong number of fields";
            break;
        case ErrorType::INDEX_OUT_OF_RANGE:
            msg_ = "index out of range";
            break;
        case ErrorType::NO_INDEX:
            msg_ = "no index";
            break;
        case ErrorType::INDEX_CLASH:
            msg_ = "index clash";
            break;
        case ErrorType::TYPE_MISMATCH:
            msg_ = "type mismatch";
            break;
        case ErrorType::INVALID_CONVERSION:
            msg_ = "invalid conversion";
            break;
        case ErrorType::IO_ERROR:
            msg_ = "invalid conversion";
            break;
        default:
            msg_ = "unknown error";
            break;
        }

        if (!msg.empty())
        {
            msg_ += ": " + msg;
        }
    }

    ~Exception() = default;

    const char* what() const noexcept override
    {
        return msg_.c_str();
    }

private:
    std::string msg_;
};

} // namespace casket::db