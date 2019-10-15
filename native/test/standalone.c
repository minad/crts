#include "test.h"

static char buf[512], buf2[512];

void* x_memset(void*, int, size_t);
int x_memcmp(const void*, const void*, size_t);

TEST(x_memset) {
    for (size_t len = 0; len < sizeof (buf) - 2; ++len) {
        for (size_t off = 1; off < sizeof (buf) - 2 - len; ++off) {
            buf[off - 1] = 0;
            buf[off + len] = 0;
            x_memset(buf + off, (char)len, len);
            for (size_t i = 0; i < len; ++i)
                ASSERTEQ(buf[off + i], (char)len);
            ASSERTEQ(buf[off - 1], 0);
            ASSERTEQ(buf[off + len], 0);
        }
    }
}

TEST(x_memcmp) {
    for (size_t len = 1; len < sizeof (buf); ++len) {
        for (size_t off = 0; off < sizeof (buf) - len; ++off) {
            memset(buf + off, (char)len, len);
            memset(buf2 + off, (char)len, len);
            ASSERT(!x_memcmp(buf + off, buf2 + off, len));
            ASSERT(!memcmp(buf + off, buf2 + off, len));
            buf[off + len - 1] = 0;
            buf2[off + len - 1] = 42;
            ASSERT(x_memcmp(buf + off, buf2 + off, len) == -42);
            ASSERT(memcmp(buf + off, buf2 + off, len) < 0);
            buf[off + len - 1] = 42;
            buf2[off + len - 1] = 0;
            ASSERT(x_memcmp(buf + off, buf2 + off, len) == 42);
            ASSERT(memcmp(buf + off, buf2 + off, len) > 0);
        }
    }
}

BENCH(memset, 10000) {
    memset(buf + (BENCHRUN % 16), 123, BENCHRUN % (sizeof (buf) - 16));
}

BENCH(x_memset, 10000) {
    x_memset(buf + (BENCHRUN % 16), 123, BENCHRUN % (sizeof (buf) - 16));
}

BENCH(memcmp, 10000) {
    __volatile__ int x = memcmp(buf + (BENCHRUN % 16), buf2 + (BENCHRUN % 16), BENCHRUN % (sizeof (buf) - 16));
    (void)x;
}

BENCH(x_memcmp, 10000) {
    __volatile__ int x = x_memcmp(buf + (BENCHRUN % 16), buf2 + (BENCHRUN % 16), BENCHRUN % (sizeof (buf) - 16));
    (void)x;
}
