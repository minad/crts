#pragma once

#include <chili/object/string.h>
#include "xxh.h"

CHI_NEWTYPE(HashIndex, size_t)

CHI_WU ChiHashIndex chiHashBytes(const void*, size_t);

CHI_INL CHI_WU ChiHashIndex chiHash32(uint32_t x) {
    return (ChiHashIndex){ xxh32Mix(x) };
}

CHI_INL CHI_WU ChiHashIndex chiHash64(uint64_t x) {
    return (ChiHashIndex){ (size_t)xxh64Mix(x) };
}

CHI_INL CHI_WU ChiHashIndex chiHashWord(ChiWord x) {
    return chiHash64(x);
}

CHI_INL CHI_WU ChiHashIndex chiHashPtr(const void* p) {
    return sizeof (uintptr_t) == 4 ? chiHash32((uint32_t)(uintptr_t)p) : chiHash64((uintptr_t)p);
}

CHI_INL CHI_WU ChiHashIndex chiHashChili(Chili c) {
    return chiHashWord(CHILI_UN(c));
}

CHI_INL CHI_WU ChiHashIndex chiHashStringRef(ChiStringRef s) {
    return chiHashBytes(s.bytes, s.size);
}
