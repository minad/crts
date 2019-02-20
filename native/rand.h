#pragma once

#include "num.h"

typedef uint64_t ChiRandState[4];

// jsf64
CHI_INL uint64_t chiRand(ChiRandState s) {
    uint64_t e = s[0] - chiRotl64(s[1], 7);
    s[0] = s[1] ^ chiRotl64(s[2], 13);
    s[1] = s[2] + chiRotl64(s[3], 37);
    s[2] = s[3] + e;
    s[3] = e + s[0];
    return s[3];
}

CHI_INL void chiRandInit(ChiRandState s, uint64_t seed) {
    s[0] = 0xf1ea5eed, s[1] = s[2] = s[3] = seed;
    for (uint32_t i = 0; i < 20; ++i)
        chiRand(s);
}
