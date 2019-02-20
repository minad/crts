#pragma once
#include "ascii.h"

enum { CHI_INT_BUFSIZE = 24 };

extern const uint64_t chiPow10[];
extern const char chiHexDigit[];

char* chiShowUInt(char*, uint64_t);
char* chiShowInt(char*, int64_t);
char* chiShowHexUInt(char*, uint64_t);
CHI_WU uint64_t chiPow(uint64_t, uint32_t);

CHI_INL CHI_WU uint32_t chiLog2(uint64_t x) {
    CHI_ASSERT(x);
    return 63 - (uint32_t)__builtin_clzll(x);
}

CHI_INL CHI_WU bool chiIsPow2(uint64_t x) {
    return x && !(x & (x - 1));
}

CHI_INL CHI_WU uint64_t chiRotl64(uint64_t x, uint32_t r) {
    return (x << r) | (x >> (64 - r));
}

CHI_INL CHI_WU uint32_t chiRotl32(uint32_t x, uint32_t r) {
    return (x << r) | (x >> (32 - r));
}

// Non locale dependent version of strtoull.
// Also doesn't use errno.
#define _CHI_READ_UINT(n)                                       \
    CHI_WU CHI_INL uint##n##_t chiReadUInt##n(const char** end) {  \
        const char* s = *end;                                   \
        uint##n##_t x = 0;                                      \
        while (chiDigit(*s))                                    \
            x = x * 10 + (uint##n##_t)(*s++ - '0');             \
        *end = s;                                               \
        return x;                                               \
    }
_CHI_READ_UINT(64)
_CHI_READ_UINT(32)
