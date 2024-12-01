#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace casket::utils
{

std::string hexlify(const std::uint8_t* bytes, std::size_t size);

std::vector<uint8_t> unhexlify(std::string_view in);

} // namespace casket::utils