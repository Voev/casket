#pragma once
#include <casket/nonstd/string_view.hpp>

namespace casket
{

class Logger
{
public:
    Logger() = default;
    virtual ~Logger() = default;

    virtual void emergency(nonstd::string_view msg) = 0;
    virtual void alert(nonstd::string_view msg) = 0;
    virtual void critical(nonstd::string_view msg) = 0;
    virtual void error(nonstd::string_view msg) = 0;
    virtual void warning(nonstd::string_view msg) = 0;
    virtual void notice(nonstd::string_view msg) = 0;
    virtual void info(nonstd::string_view msg) = 0;
    virtual void debug(nonstd::string_view msg) = 0;
};

} // namespace casket