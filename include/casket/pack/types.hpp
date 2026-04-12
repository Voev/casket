#pragma once
#include <cstdint>

namespace casket
{

enum class TypeTag : uint8_t
{
    Nil = 0xC0,
    False = 0xC2,
    True = 0xC3,
    Int8 = 0xD0,
    Int16 = 0xD1,
    Int32 = 0xD2,
    Int64 = 0xD3,
    UInt8 = 0xCC,
    UInt16 = 0xCD,
    UInt32 = 0xCE,
    UInt64 = 0xCF,
    Float32 = 0xCA,
    Float64 = 0xCB,
    Str8 = 0xD9,
    Str16 = 0xDA,
    Str32 = 0xDB,
    Array16 = 0xDC,
    Array32 = 0xDD,
    Map16 = 0xDE,
    Map32 = 0xDF
};

} // namespace casket