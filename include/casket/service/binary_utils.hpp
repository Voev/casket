#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <string>
#include <algorithm>

namespace casket
{

using Handler = std::function<void(const std::vector<uint8_t>&, std::vector<uint8_t>&)>;

inline std::vector<uint8_t> StringToBinary(const std::string& str)
{
    return std::vector<uint8_t>(str.begin(), str.end());
}

inline std::string BinaryToString(const std::vector<uint8_t>& binary)
{
    return std::string(binary.begin(), binary.end());
}

inline std::vector<uint8_t> CreateBinary(const void* data, size_t size)
{
    const uint8_t* byteData = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(byteData, byteData + size);
}

template <typename T>
std::vector<uint8_t> ToBinary(const T& value)
{
    const uint8_t* byteData = reinterpret_cast<const uint8_t*>(&value);
    return std::vector<uint8_t>(byteData, byteData + sizeof(T));
}

} // namespace casket