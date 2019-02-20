#pragma once

#include "private.h"

// 8 byte words, even on 32 bit platforms!
CHI_INL void chiCopyWords(void *dst, const void *src, size_t n) {
    ChiWord* d = (ChiWord*)dst;
    const ChiWord* s = (const ChiWord*)src;
    while (n >= 8) {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        n -= 8;
    }
    switch (n) {
    case 7: *d++ = *s++; // fallthrough
    case 6: *d++ = *s++; // fallthrough
    case 5: *d++ = *s++; // fallthrough
    case 4: *d++ = *s++; // fallthrough
    case 3: *d++ = *s++; // fallthrough
    case 2: *d++ = *s++; // fallthrough
    case 1: *d++ = *s++;
    }
}
