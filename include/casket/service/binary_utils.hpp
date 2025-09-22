#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <string>
#include <algorithm>

namespace casket
{

using BinaryRequest = std::vector<uint8_t>;
using BinaryResponse = std::vector<uint8_t>;
using BinaryHandler = std::function<void(const BinaryRequest&, BinaryResponse&)>;

inline BinaryRequest StringToBinary(const std::string& str)
{
    return BinaryRequest(str.begin(), str.end());
}

inline std::string BinaryToString(const BinaryRequest& binary)
{
    return std::string(binary.begin(), binary.end());
}

inline BinaryRequest CreateBinaryRequest(const void* data, size_t size)
{
    const uint8_t* byteData = static_cast<const uint8_t*>(data);
    return BinaryRequest(byteData, byteData + size);
}

template <typename T>
BinaryRequest to_binary(const T& value)
{
    const uint8_t* byteData = reinterpret_cast<const uint8_t*>(&value);
    return BinaryRequest(byteData, byteData + sizeof(T));
}

} // namespace casket