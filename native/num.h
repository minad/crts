#pragma once
#include "ascii.h"

enum { CHI_INT_BUFSIZE = 24 };

CHI_EXTERN const uint64_t chiPow10[];
CHI_EXTERN const char chiHexDigits[];

CHI_INTERN char* chiShowUInt(char*, uint64_t);
CHI_INTERN char* chiShowInt(char*, int64_t);
CHI_INTERN char* chiShowHexUInt(char*, uint64_t);
CHI_INTERN CHI_WU uint64_t chiPow(uint64_t, uint32_t);
CHI_INTERN CHI_WU bool chiReadUInt32(uint32_t*, const char**);
CHI_INTERN CHI_WU bool chiReadUInt64(uint64_t*, const char**);

CHI_INL CHI_WU uint32_t chiLog2Floor(uint64_t x) {
    CHI_ASSERT(x > 0);
    return 63 - (uint32_t)__builtin_clzll(x);
}

CHI_INL CHI_WU uint32_t chiLog2Ceil(uint64_t x) {
    CHI_ASSERT(x > 1);
    return 64 - (uint32_t)__builtin_clzll(x - 1);
}

CHI_INL CHI_WU bool chiIsPow2(uint64_t x) {
    return x && !(x & (x - 1));
}

CHI_INL CHI_WU uint64_t chiRotl64(uint64_t x, uint32_t r) {
    CHI_ASSERT(r < 64);
    return (x << r) | (x >> (64 - r));
}

CHI_INL CHI_WU uint32_t chiRotl32(uint32_t x, uint32_t r) {
    CHI_ASSERT(r < 32);
    return (x << r) | (x >> (32 - r));
}
