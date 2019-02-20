#include <chili/object/string.h>
#include <stdlib.h>
#include "test.h"
#include "../strutil.h"
#include "../mem.h"

BENCH(newSmallFixed, 1000000) {
    Chili c = chiNew(CHI_RAW, 8);
    *(__volatile__ uint8_t*)_chiRawPayload(c) = 1;
}

BENCH(mallocSmallFixed, 1000000) {
    void* p = chiAlloc(64);
    *(__volatile__ uint8_t*)p = 1;
}

BENCH(mallocFreeSmallFixed, 1000000) {
    void* p = chiAlloc(64);
    *(__volatile__ uint8_t*)p = 1;
    chiFree(p);
}

BENCH(newSmall, 1000000) {
    size_t s;
    do {
        s = (size_t)RAND % 32;
    } while (!s);
    Chili c = chiNew(CHI_RAW, s);
    *(__volatile__ uint8_t*)_chiRawPayload(c) = 1;
}

BENCH(mallocSmall, 1000000) {
    size_t s;
    do {
        s = (size_t)RAND % 32;
    } while (!s);
    void* p = chiAllocArr(ChiWord, s);
    *(__volatile__ uint8_t*)p = 1;
}

BENCH(mallocFreeSmall, 1000000) {
    size_t s;
    do {
        s = (size_t)RAND % 32;
    } while (!s);
    void* p = chiAllocArr(ChiWord, s);
    *(__volatile__ uint8_t*)p = 1;
    chiFree(p);
}

BENCH(newLarge, 10000) {
    size_t s;
    do {
        s = (size_t)RAND % 1024 + 1024;
    } while (!s);
    Chili c = chiNew(CHI_RAW, s);
    *(__volatile__ uint8_t*)_chiRawPayload(c) = 1;
}

BENCH(mallocLarge, 10000) {
    size_t s;
    do {
        s = (size_t)RAND % 1024 + 1024;
    } while (!s);
    void* p = chiAllocArr(ChiWord, s);
    *(__volatile__ uint8_t*)p = 1;
}

BENCH(mallocFreeLarge, 10000) {
    size_t s;
    do {
        s = (size_t)RAND % 1024 + 1024;
    } while (!s);
    void* p = chiAllocArr(ChiWord, s);
    *(__volatile__ uint8_t*)p = 1;
    chiFree(p);
}

TEST(chiNew) {
    for (size_t s = 1; s < 1024; ++s) {
        Chili c = chiNew(CHI_RAW, s);
        ASSERT(s <= CHI_MAX_UNPINNED ? !chiPinned(c) : chiPinned(c));
        ASSERTEQ(chiSize(c), s);
    }
    for (size_t i = 1; i < 100; ++i) {
        size_t s;
        do {
            s = (size_t)RAND % (1024 * 128);
        } while (!s);
        Chili c = chiNew(CHI_RAW, s);
        ASSERT(s <= CHI_MAX_UNPINNED ? !chiPinned(c) : chiPinned(c));
        ASSERTEQ(chiSize(c), s);
    }
}

TEST(chiNewPin) {
    for (size_t s = 1; s < 1024; ++s) {
        Chili c = chiNewPin(CHI_RAW, s);
        ASSERT(chiPinned(c));
        ASSERTEQ(chiSize(c), s);
    }
    for (size_t i = 1; i < 100; ++i) {
        size_t s;
        do {
            s = (size_t)RAND % (1024 * 128);
        } while (!s);
        Chili c = chiNewPin(CHI_RAW, s);
        ASSERT(chiPinned(c));
        ASSERTEQ(chiSize(c), s);
    }
}

TEST(allocStress) {
    for (size_t i = 1; i < 1000; ++i) {
        size_t s;
        do {
            s = (size_t)RAND % 1024;
        } while (!s);
        Chili c;
        if (RAND % 1) {
            c = chiNew(CHI_RAW, s);
            ASSERT(s <= CHI_MAX_UNPINNED ? !chiPinned(c) : chiPinned(c));
        } else {
            c = chiNewPin(CHI_RAW, s);
            ASSERT(chiPinned(c));
        }
        ASSERTEQ(chiSize(c), s);

        Chili p = chiTryPin(c);
        ASSERT(chiPinned(p));
        ASSERTEQ(chiSize(p), s);
    }
}

TEST(chiTryPin) {
    Chili x = chiNew(CHI_RAW, 2);
    ASSERT(!chiPinned(x));
    memcpy(_chiRawPayload(x), "abcdabcd", 8);

    Chili x2 = chiTryPin(x);
    ASSERT(chiPinned(x2));
    ASSERT(!chiIdentical(x, x2));
    ASSERT(memeq(_chiRawPayload(x2), "abcdabcd", 8));

    Chili x3 = chiTryPin(x2);
    ASSERT(chiIdentical(x2, x3));
}
