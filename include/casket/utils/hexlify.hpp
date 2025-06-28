#pragma once
#include <string>
#include <casket/nonstd/string_view.hpp>
#include <vector>

namespace casket
{

std::string hexlify(const std::uint8_t* bytes, std::size_t size);

std::vector<uint8_t> unhexlify(nonstd::string_view in);

} // namespace casket