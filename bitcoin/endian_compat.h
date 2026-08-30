#ifndef BITCOIN_ENDIAN_COMPAT_H
#define BITCOIN_ENDIAN_COMPAT_H

#ifdef _WIN32

#include <stdint.h>

static inline uint16_t bitcoin_bswap16(uint16_t value)
{
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}

static inline uint32_t bitcoin_bswap32(uint32_t value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

static inline uint64_t bitcoin_bswap64(uint64_t value)
{
    return (static_cast<uint64_t>(bitcoin_bswap32(value)) << 32) |
           bitcoin_bswap32(value >> 32);
}

#ifndef htobe16
#define htobe16(x) bitcoin_bswap16(x)
#define htole16(x) (x)
#define be16toh(x) bitcoin_bswap16(x)
#define le16toh(x) (x)
#define htobe32(x) bitcoin_bswap32(x)
#define htole32(x) (x)
#define be32toh(x) bitcoin_bswap32(x)
#define le32toh(x) (x)
#define htobe64(x) bitcoin_bswap64(x)
#define htole64(x) (x)
#define be64toh(x) bitcoin_bswap64(x)
#define le64toh(x) (x)
#endif

#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#else
#include <endian.h>
#endif

#endif
