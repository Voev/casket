#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <casket/log/console.hpp>
#include <casket/log/color.hpp>

namespace
{

std::string currentTimeAndDate()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "[%X %Y-%m-%d]");
    return ss.str();
}

void cerr(nonstd::string_view color, nonstd::string_view label, nonstd::string_view msg)
{
    std::cerr << currentTimeAndDate() << "[" << color << label
              << casket::resetColor << "] " << msg << std::endl;
}

void cout(nonstd::string_view color, nonstd::string_view label, nonstd::string_view msg)
{
    std::cout << currentTimeAndDate() << "[" << color << label
              << casket::resetColor << "] " << msg << std::endl;
}

} // namespace

namespace casket
{
void Console::emergency(nonstd::string_view msg)
{
    cerr(bRed, "EMERG", msg);
}

void Console::alert(nonstd::string_view msg)
{
    cerr(bRed, "ALERT", msg);
}

void Console::critical(nonstd::string_view msg)
{
    cerr(bRed, "CRITL", msg);
}

void Console::error(nonstd::string_view msg)
{
    cerr(bRed, "ERROR", msg);
}

void Console::warning(nonstd::string_view msg)
{
    cout(bYellow, "WARNG", msg);
}

void Console::notice(nonstd::string_view msg)
{
    cout(bWhite, "NOTIC", msg);
}

void Console::info(nonstd::string_view msg)
{
    cout(bWhite, "INFOR", msg);
}

void Console::debug(nonstd::string_view msg)
{
    cout(bCyan, "DEBUG", msg);
}

} // namespace casket
