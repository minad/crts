#pragma once

#include "xxh.h"

CHI_NEWTYPE(Hash, size_t)

CHI_INTERN CHI_WU ChiHash chiHashBytes(const void*, size_t);

CHI_INL CHI_WU ChiHash chiHash32(uint32_t x) {
    x *= XXH_PRIME32_3;
    x ^= x >> 16;
    return CHI_WRAP(Hash, x);
}

CHI_INL CHI_WU ChiHash chiHash64(uint64_t x) {
    x *= XXH_PRIME64_3;
    x ^= x >> 32;
    return CHI_WRAP(Hash, (size_t)x);
}

CHI_INL CHI_WU ChiHash chiHashPtr(const void* p) {
    return sizeof (uintptr_t) == 4 ? chiHash32((uint32_t)(uintptr_t)p) : chiHash64((uintptr_t)p);
}

CHI_INL CHI_WU ChiHash chiHashStringRef(ChiStringRef s) {
    return chiHashBytes(s.bytes, s.size);
}

CHI_INL ChiHash chiHashCString(const char* s) {
    size_t h = 0;
    while (*s) {
        h ^= (size_t)*s++;
        h *= CHI_ARCH_32BIT ? XXH_PRIME32_3 : (size_t)XXH_PRIME64_3;
        h ^= h >> (CHI_ARCH_32BIT ? 16 : 32);
    }
    return CHI_WRAP(Hash, h);
}
