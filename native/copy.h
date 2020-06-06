#pragma once

#include "private.h"

// 8 byte words, even on 32 bit platforms!
CHI_INL void chiCopyWords(void *restrict dst, const void *restrict src, size_t n) {
    ChiWord* d = (ChiWord*)dst;
    const ChiWord* s = (const ChiWord*)src;
    while (n >= 4) {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        n -= 4;
    }
    switch (n) {
    case 3: *d++ = *s++; // fallthrough
    case 2: *d++ = *s++; // fallthrough
    case 1: *d++ = *s++;
    }
}
