#pragma once
#include <cstddef>

namespace casket
{

class LogSink
{
public:
    virtual ~LogSink() = default;
    virtual void emergency(const char* msg, size_t length) = 0;
    virtual void alert(const char* msg, size_t length) = 0;
    virtual void critical(const char* msg, size_t length) = 0;
    virtual void error(const char* msg, size_t length) = 0;
    virtual void warning(const char* msg, size_t length) = 0;
    virtual void notice(const char* msg, size_t length) = 0;
    virtual void info(const char* msg, size_t length) = 0;
    virtual void debug(const char* msg, size_t length) = 0;
    virtual void flush() {}
};

} // namespace casket