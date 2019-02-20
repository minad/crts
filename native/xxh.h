/*
 * NOTE: This is the xxHash implementation taken from the Linux kernel,
 * adapted for the Chili runtime. In the kernel the file is dual-licensed
 * as BSD-2-Clause and GPL-2 (see below). This implementation is a bit smaller
 * than the original xxHash implementation by Yann Collet.
 *
 * xxHash - Extremely Fast Hash algorithm
 * Copyright (C) 2012-2016, Yann Collet.
 *
 * BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation. This program is dual-licensed; you may select
 * either version 2 of the GNU General Public License ("GPL") or BSD license
 * ("BSD").
 *
 * You can contact the author at:
 * - xxHash homepage: http://cyan4973.github.io/xxHash/
 * - xxHash source repository: https://github.com/Cyan4973/xxHash
 */

#pragma once

#include "num.h"

static const uint32_t XXH_PRIME32_1 = 2654435761U;
static const uint32_t XXH_PRIME32_2 = 2246822519U;
static const uint32_t XXH_PRIME32_3 = 3266489917U;
static const uint32_t XXH_PRIME32_4 = 668265263U;
static const uint32_t XXH_PRIME32_5 = 374761393U;

static const uint64_t XXH_PRIME64_1 = 11400714785074694791ULL;
static const uint64_t XXH_PRIME64_2 = 14029467366897019727ULL;
static const uint64_t XXH_PRIME64_3 = 1609587929392839161ULL;
static const uint64_t XXH_PRIME64_4 = 9650029242287828579ULL;
static const uint64_t XXH_PRIME64_5 = 2870177450012600261ULL;

CHI_INL CHI_WU uint64_t xxh64Mix(uint64_t x) {
    x ^= x >> 33;
    x *= XXH_PRIME64_2;
    x ^= x >> 29;
    x *= XXH_PRIME64_3;
    x ^= x >> 32;
    return x;
}

CHI_INL CHI_WU uint32_t xxh32Mix(uint32_t x) {
    x ^= x >> 15;
    x *= XXH_PRIME32_2;
    x ^= x >> 13;
    x *= XXH_PRIME32_3;
    x ^= x >> 16;
    return x;
}

CHI_INL CHI_WU uint32_t xxh32Round(uint32_t seed, const uint32_t input) {
    seed += input * XXH_PRIME32_2;
    seed = chiRotl32(seed, 13);
    seed *= XXH_PRIME32_1;
    return seed;
}

CHI_INL CHI_WU uint32_t xxh32(const void *input, const size_t len, const uint32_t seed) {
    const uint8_t *p = (const uint8_t *)input;
    const uint8_t *b_end = p + len;
    uint32_t h;

    if (len >= 16) {
        const uint8_t *const limit = b_end - 16;
        uint32_t v1 = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
        uint32_t v2 = seed + XXH_PRIME32_2;
        uint32_t v3 = seed + 0;
        uint32_t v4 = seed - XXH_PRIME32_1;

        do {
            v1 = xxh32Round(v1, chiPeekUInt32(p));
            p += 4;
            v2 = xxh32Round(v2, chiPeekUInt32(p));
            p += 4;
            v3 = xxh32Round(v3, chiPeekUInt32(p));
            p += 4;
            v4 = xxh32Round(v4, chiPeekUInt32(p));
            p += 4;
        } while (p <= limit);

        h = chiRotl32(v1, 1) + chiRotl32(v2, 7) +
            chiRotl32(v3, 12) + chiRotl32(v4, 18);
    } else {
        h = seed + XXH_PRIME32_5;
    }

    h += (uint32_t)len;

    while (p + 4 <= b_end) {
        h += chiPeekUInt32(p) * XXH_PRIME32_3;
        h = chiRotl32(h, 17) * XXH_PRIME32_4;
        p += 4;
    }

    while (p < b_end) {
        h += (*p) * XXH_PRIME32_5;
        h = chiRotl32(h, 11) * XXH_PRIME32_1;
        p++;
    }

    return xxh32Mix(h);
}

CHI_INL CHI_WU uint64_t xxh64Round(uint64_t acc, const uint64_t input) {
    acc += input * XXH_PRIME64_2;
    acc = chiRotl64(acc, 31);
    acc *= XXH_PRIME64_1;
    return acc;
}

CHI_INL CHI_WU uint64_t xxh64MergeRound(uint64_t acc, uint64_t val) {
    val = xxh64Round(0, val);
    acc ^= val;
    acc = acc * XXH_PRIME64_1 + XXH_PRIME64_4;
    return acc;
}

CHI_INL CHI_WU uint64_t xxh64(const void *input, const size_t len, const uint64_t seed) {
    const uint8_t *p = (const uint8_t *)input;
    const uint8_t *const b_end = p + len;
    uint64_t h;

    if (len >= 32) {
        const uint8_t *const limit = b_end - 32;
        uint64_t v1 = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
        uint64_t v2 = seed + XXH_PRIME64_2;
        uint64_t v3 = seed + 0;
        uint64_t v4 = seed - XXH_PRIME64_1;

        do {
            v1 = xxh64Round(v1, chiPeekUInt64(p));
            p += 8;
            v2 = xxh64Round(v2, chiPeekUInt64(p));
            p += 8;
            v3 = xxh64Round(v3, chiPeekUInt64(p));
            p += 8;
            v4 = xxh64Round(v4, chiPeekUInt64(p));
            p += 8;
        } while (p <= limit);

        h = chiRotl64(v1, 1) + chiRotl64(v2, 7) +
            chiRotl64(v3, 12) + chiRotl64(v4, 18);
        h = xxh64MergeRound(h, v1);
        h = xxh64MergeRound(h, v2);
        h = xxh64MergeRound(h, v3);
        h = xxh64MergeRound(h, v4);

    } else {
        h  = seed + XXH_PRIME64_5;
    }

    h += (uint64_t)len;

    while (p + 8 <= b_end) {
        const uint64_t k1 = xxh64Round(0, chiPeekUInt64(p));

        h ^= k1;
        h = chiRotl64(h, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
        p += 8;
    }

    if (p + 4 <= b_end) {
        h ^= (uint64_t)chiPeekUInt32(p) * XXH_PRIME64_1;
        h = chiRotl64(h, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
        p += 4;
    }

    while (p < b_end) {
        h ^= (*p) * XXH_PRIME64_5;
        h = chiRotl64(h, 11) * XXH_PRIME64_1;
        p++;
    }

    return xxh64Mix(h);
}
