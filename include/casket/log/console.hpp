#pragma once
#include <casket/log/logger.hpp>

namespace casket
{

class Console final : public Logger
{
public:
    Console() = default;

    ~Console() = default;

    void emergency(nonstd::string_view msg) override;

    void alert(nonstd::string_view msg) override;

    void critical(nonstd::string_view msg) override;

    void error(nonstd::string_view msg) override;

    void warning(nonstd::string_view msg) override;

    void notice(nonstd::string_view msg) override;

    void info(nonstd::string_view msg) override;

    void debug(nonstd::string_view msg) override;
};

} // namespace casket