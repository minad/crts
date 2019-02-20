#include "../num.h"
#include "../strutil.h"
#include "test.h"
#include <stdio.h>

TEST(chiShowUInt) {
    char a[24], b[24];
    for (uint64_t i = 0; i < 10000; ++i) {
        char* end = chiShowUInt(a, i);
        snprintf(b, sizeof (b), "%ju", i);
        ASSERT(end == a + strlen(a));
        ASSERT(streq(a, b));
    }
    for (uint32_t i = 0; i < 10000; ++i) {
        uint64_t x = RAND;
        char* end = chiShowUInt(a, x);
        snprintf(b, sizeof (b), "%ju", x);
        ASSERT(end == a + strlen(a));
        ASSERT(streq(a, b));
    }
}

TEST(chiShowInt) {
    char a[24], b[24];
    for (int64_t i = -10000; i < 10000; ++i) {
        char* end = chiShowInt(a, i);
        snprintf(b, sizeof (b), "%jd", i);
        ASSERT(end == a + strlen(a));
        ASSERT(streq(a, b));
    }
    for (uint32_t i = 0; i < 10000; ++i) {
        int64_t x = (int64_t)RAND;
        char* end = chiShowInt(a, x);
        snprintf(b, sizeof (b), "%jd", x);
        ASSERT(end == a + strlen(a));
        ASSERT(streq(a, b));
    }
}

TEST(chiShowHexUInt) {
    char a[24], b[24];
    for (uint64_t i = 0; i < 10000; ++i) {
        char* end = chiShowHexUInt(a, i);
        snprintf(b, sizeof (b), "%jx", i);
        ASSERT(end == a + strlen(a));
        ASSERT(streq(a, b));
    }
    for (uint32_t i = 0; i < 10000; ++i) {
        uint64_t x = RAND;
        char* end = chiShowHexUInt(a, x);
        snprintf(b, sizeof (b), "%jx", x);
        ASSERT(end == a + strlen(a));
        ASSERT(streq(a, b));
    }
}

BENCH(chiShowUInt, 10000) {
    char a[24];
    chiShowUInt(a, RAND);
}
