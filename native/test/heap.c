#include <stdlib.h>
#include "../mem.h"
#include "../new.h"
#include "../object.h"
#include "../strutil.h"
#include "test.h"

BENCH(newSmallFixed, 1000000) {
    Chili c = chiNew(CHI_PRESTRING, 8);
    *(__volatile__ uint8_t*)chiRawPayload(c) = 1;
}

BENCH(newSmallFixedLoc, 1000000) {
    Chili c = chiNewFlags(CHI_PRESTRING, 8, CHI_NEW_LOCAL);
    *(__volatile__ uint8_t*)chiRawPayload(c) = 1;
}

BENCH(newSmallFixedLocInl, 1000000) {
    Chili c = chiNewInl(CHI_CURRENT_PROCESSOR, CHI_PRESTRING, 8, CHI_NEW_LOCAL);
    *(__volatile__ uint8_t*)chiRawPayload(c) = 1;
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
    Chili c = chiNew(CHI_PRESTRING, s);
    *(__volatile__ uint8_t*)chiRawPayload(c) = 1;
}

BENCH(newSmallLoc, 1000000) {
    size_t s;
    do {
        s = (size_t)RAND % 32;
    } while (!s);
    Chili c = chiNewFlags(CHI_PRESTRING, s, CHI_NEW_LOCAL);
    *(__volatile__ uint8_t*)chiRawPayload(c) = 1;
}

BENCH(newSmallLocInl, 1000000) {
    size_t s;
    do {
        s = (size_t)RAND % 32;
    } while (!s);
    Chili c = chiNewInl(CHI_CURRENT_PROCESSOR, CHI_PRESTRING, s, CHI_NEW_LOCAL);
    *(__volatile__ uint8_t*)chiRawPayload(c) = 1;
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
    Chili c = chiNew(CHI_PRESTRING, s);
    *(__volatile__ uint8_t*)chiRawPayload(c) = 1;
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

SUBTEST(chiNew, ChiType type, bool shared) {
    for (size_t s = 1; s < 1024; ++s) {
        Chili c = chiNew(type, s);
        ASSERT(chiGen(c) == (s <= CHI_MAX_UNPINNED ? CHI_GEN_NURSERY : CHI_GEN_MAJOR));
        ASSERTEQ(chiSize(c), s);
        ASSERT(chiGen(c) != CHI_GEN_MAJOR || shared == chiObjectShared(chiObject(c)));
    }
    for (size_t i = 1; i < 100; ++i) {
        size_t s;
        do {
            s = (size_t)RAND % (1024 * 128);
        } while (!s);
        Chili c = chiNew(type, s);
        ASSERT(chiGen(c) == (s <= CHI_MAX_UNPINNED ? CHI_GEN_NURSERY : CHI_GEN_MAJOR));
        ASSERTEQ(chiSize(c), s);
        ASSERT(chiGen(c) != CHI_GEN_MAJOR || shared == chiObjectShared(chiObject(c)));
    }
}

SUBTEST(chiNewFlags, ChiType type, ChiNewFlags flags, bool shared) {
    for (size_t s = 1; s < 1024; ++s) {
        Chili c = chiNewFlags(type, s, flags);
        ASSERT(chiGen(c) == CHI_GEN_MAJOR);
        ASSERTEQ(chiSize(c), s);
        ASSERT(chiGen(c) != CHI_GEN_MAJOR || shared == chiObjectShared(chiObject(c)));
    }
    for (size_t i = 1; i < 100; ++i) {
        size_t s;
        do {
            s = (size_t)RAND % (1024 * 128);
        } while (!s);
        Chili c = chiNewFlags(type, s, flags);
        ASSERT(chiGen(c) == CHI_GEN_MAJOR);
        ASSERTEQ(chiSize(c), s);
        ASSERT(chiGen(c) != CHI_GEN_MAJOR || shared == chiObjectShared(chiObject(c)));
    }
}

TEST(chiNew) {
    RUNSUBTEST(chiNew, CHI_PRESTRING, true);
    RUNSUBTEST(chiNew, CHI_TAG(0), false);
}

TEST(chiNewFlags) {
    RUNSUBTEST(chiNewFlags, CHI_PRESTRING, CHI_NEW_SHARED | CHI_NEW_CLEAN, true);
    RUNSUBTEST(chiNewFlags, CHI_TAG(0), CHI_NEW_LOCAL, false);
    RUNSUBTEST(chiNewFlags, CHI_TAG(0), CHI_NEW_SHARED | CHI_NEW_CLEAN, true);
}

TEST(allocStress) {
    for (size_t i = 1; i < 1000; ++i) {
        size_t s;
        do {
            s = (size_t)RAND % 1024;
        } while (!s);
        Chili c;
        if (RAND % 1) {
            c = chiNew(CHI_PRESTRING, s);
            ASSERT(chiGen(c) == (s <= CHI_MAX_UNPINNED ? CHI_GEN_NURSERY : CHI_GEN_MAJOR));
        } else {
            c = chiNewFlags(CHI_PRESTRING, s, CHI_NEW_SHARED | CHI_NEW_CLEAN);
            ASSERT(chiGen(c) == CHI_GEN_MAJOR);
        }
        ASSERTEQ(chiSize(c), s);
    }
}
