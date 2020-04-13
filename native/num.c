#include "num.h"

/*
 * See https://github.com/miloyip/itoa-benchmark
 */

static const char digits[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
};

CHI_INTERN const uint64_t chiPow10[] = { 1e0,1e1,1e2,1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11,1e12,1e13,1e14,1e15,1e16,1e17,1e18,1e19 };
CHI_INTERN const char chiHexDigits[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

static uint32_t countDigits10(uint64_t n) {
    if (n == 0)
        return 1;
    uint32_t t = (uint32_t)(64 - __builtin_clzll(n)) * 1233 >> 12;
    return t - (n < chiPow10[t]) + 1;
}

static uint32_t countDigits16(uint64_t n) {
    return n < 16 ? 1 : 16 - (uint32_t)(__builtin_clzll(n | 1) >> 2);
}

char* chiShowUInt(char* p, uint64_t n) {
    p += countDigits10(n);
    char *end = p;
    *end = 0;
    while (n >= 10000) {
        uint64_t a = n % 10000, b = 2 * (a % 100), c = 2 * (a / 100);
        n /= 10000;
        memcpy(p -= 2, digits + b, 2);
        memcpy(p -= 2, digits + c, 2);
    }
    while (n >= 100) {
        uint64_t a = 2 * (n % 100);
        n /= 100;
        memcpy(p -= 2, digits + a, 2);
    }
    if (n < 10) {
        p[-1] = (char)('0' + n);
    } else {
        n *= 2;
        memcpy(p - 2, digits + n, 2);
    }
    return end;
}

char* chiShowInt(char* p, int64_t n) {
    if (n < 0) {
        *p++ = '-';
        n = -n;
    }
    return chiShowUInt(p, (uint64_t)n);
}

char* chiShowHexUInt(char* p, uint64_t n) {
    p += countDigits16(n);
    char *end = p;
    *end = 0;
    while (n >= 256) {
        *--p = chiHexDigits[n & 15];
        *--p = chiHexDigits[(n / 16) & 15];
        n /= 256;
    }
    *--p = chiHexDigits[n & 15];
    if (n >= 16)
        *--p = chiHexDigits[(n / 16) & 15];
    return end;
}

uint64_t chiPow(uint64_t a, uint32_t b) {
    uint64_t p = 1;
    while (b) {
        if (b & 1)
            p *= a;
        b >>= 1;
        a *= a;
    }
    return p;
}

// Non locale dependent version of strtoull.
// Also doesn't use errno.
#define CHI_READ_UINT(n)                                               \
    bool chiReadUInt##n(uint##n##_t* x, const char** end) {             \
        const char* s = *end;                                           \
        *x = 0;                                                         \
        while (chiDigit(*s)) {                                          \
            if (__builtin_mul_overflow(*x, 10, x) ||                    \
                __builtin_add_overflow(*x, (uint##n##_t)(*s++ - '0'), x)) \
                return false;                                           \
        }                                                               \
        if (*end == s)                                                  \
            return false;                                               \
        *end = s;                                                       \
        return true;                                                    \
    }
CHI_READ_UINT(64)
CHI_READ_UINT(32)
#undef CHI_READ_UINT
