#include <stdio.h>
#include "../num.h"
#include "../strutil.h"
#include "test.h"

TEST(chiShowUInt) {
    char a[CHI_INT_BUFSIZE], b[CHI_INT_BUFSIZE];
    for (uint64_t i = 0; i < 10000; ++i) {
        *chiShowUInt(a, i) = 0;
        snprintf(b, sizeof (b), "%ju", i);
        ASSERT(streq(a, b));
    }
    for (uint32_t i = 0; i < 10000; ++i) {
        uint64_t x = RAND;
        *chiShowUInt(a, x) = 0;
        snprintf(b, sizeof (b), "%ju", x);
        ASSERT(streq(a, b));
    }
}

TEST(chiShowInt) {
    char a[CHI_INT_BUFSIZE], b[CHI_INT_BUFSIZE];
    for (int64_t i = -10000; i < 10000; ++i) {
        *chiShowInt(a, i) = 0;
        snprintf(b, sizeof (b), "%jd", i);
        ASSERT(streq(a, b));
    }
    for (uint32_t i = 0; i < 10000; ++i) {
        int64_t x = (int64_t)RAND;
        *chiShowInt(a, x) = 0;
        snprintf(b, sizeof (b), "%jd", x);
        ASSERT(streq(a, b));
    }
}

TEST(chiShowHexUInt) {
    char a[CHI_INT_BUFSIZE], b[CHI_INT_BUFSIZE];
    for (uint64_t i = 0; i < 10000; ++i) {
        *chiShowHexUInt(a, i) = 0;
        snprintf(b, sizeof (b), "%jx", i);
        ASSERT(streq(a, b));
    }
    for (uint32_t i = 0; i < 10000; ++i) {
        uint64_t x = RAND;
        *chiShowHexUInt(a, x) = 0;
        snprintf(b, sizeof (b), "%jx", x);
        ASSERT(streq(a, b));
    }
}

BENCH(chiShowUInt, 10000) {
    char a[CHI_INT_BUFSIZE];
    chiShowUInt(a, RAND);
}
