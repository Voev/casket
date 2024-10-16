#pragma once

#include <cstdint>
#include <casket/utils/byteswap.hpp>

#if defined(__APPLE__)
#include <sys/types.h>
#define casket_IS_LITTLE_ENDIAN (BYTE_ORDER == LITTLE_ENDIAN)
#define casket_IS_BIG_ENDIAN (BYTE_ORDER == BIG_ENDIAN)
#elif defined(BSD)
#include <sys/endian.h>
#define casket_IS_LITTLE_ENDIAN (_BYTE_ORDER == _LITTLE_ENDIAN)
#define casket_IS_BIG_ENDIAN (_BYTE_ORDER == _BIG_ENDIAN)
#elif defined(_WIN32)
#include <cstdlib>
#define casket_IS_LITTLE_ENDIAN 1
#define casket_IS_BIG_ENDIAN 0
#else
#include <endian.h>
#define casket_IS_LITTLE_ENDIAN (__BYTE_ORDER == __LITTLE_ENDIAN)
#define casket_IS_BIG_ENDIAN (__BYTE_ORDER == __BIG_ENDIAN)
#endif

namespace casket::utils
{

inline uint8_t do_change_endian(uint8_t data)
{
    return data;
}

inline uint16_t do_change_endian(uint16_t data)
{
    return bswap_16(data);
}

inline uint32_t do_change_endian(uint32_t data)
{
    return bswap_32(data);
}

inline uint64_t do_change_endian(uint64_t data)
{
    return bswap_64(data);
}

template <typename T> struct conversion_dispatch_helper
{
    static T dispatch(T data)
    {
        return do_change_endian(data);
    }
};

template <std::size_t> struct conversion_dispatcher;

template <>
struct conversion_dispatcher<sizeof(uint8_t)>
    : public conversion_dispatch_helper<uint8_t>
{
};

template <>
struct conversion_dispatcher<sizeof(uint16_t)>
    : public conversion_dispatch_helper<uint16_t>
{
};

template <>
struct conversion_dispatcher<sizeof(uint32_t)>
    : public conversion_dispatch_helper<uint32_t>
{
};

template <>
struct conversion_dispatcher<sizeof(uint64_t)>
    : public conversion_dispatch_helper<uint64_t>
{
};

template <typename T> inline T change_endian(T data)
{
    return conversion_dispatcher<sizeof(T)>::dispatch(data);
}

#if defined(casket_IS_LITTLE_ENDIAN)

template <typename T> inline T host_to_be(T data)
{
    return change_endian(data);
}

template <typename T> inline T host_to_le(T data)
{
    return data;
}

template <typename T> inline T be_to_host(T data)
{
    return change_endian(data);
}

template <typename T> inline T le_to_host(T data)
{
    return data;
}

#elif defined(casket_IS_BIG_ENDIAN)

template <typename T> inline T host_to_be(T data)
{
    return data;
}

template <typename T> inline T host_to_le(T data)
{
    return change_endian(data);
}

template <typename T> inline T be_to_host(T data)
{
    return data;
}

template <typename T> inline T le_to_host(T data)
{
    return change_endian(data);
}
#endif

} // namespace casket::utils
