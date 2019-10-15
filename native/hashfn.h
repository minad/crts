#pragma once

#include "xxh.h"

CHI_NEWTYPE(Hash, size_t)

CHI_INTERN CHI_WU ChiHash chiHashBytes(const void*, size_t);

CHI_INL CHI_WU ChiHash chiHash32(uint32_t x) {
    return CHI_WRAP(Hash, xxh32Mix(x));
}

CHI_INL CHI_WU ChiHash chiHash64(uint64_t x) {
    return CHI_WRAP(Hash, (size_t)xxh64Mix(x));
}

CHI_INL CHI_WU ChiHash chiHashWord(ChiWord x) {
    return chiHash64(x);
}

CHI_INL CHI_WU ChiHash chiHashPtr(const void* p) {
    return sizeof (uintptr_t) == 4 ? chiHash32((uint32_t)(uintptr_t)p) : chiHash64((uintptr_t)p);
}

CHI_INL CHI_WU ChiHash chiHashChili(Chili c) {
    return chiHashWord(CHILI_UN(c));
}

CHI_INL CHI_WU ChiHash chiHashStringRef(ChiStringRef s) {
    return chiHashBytes(s.bytes, s.size);
}
