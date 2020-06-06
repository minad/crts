#include <stdio.h>
#include "../strutil.h"
#include "test.h"

static ChiSinkMem sink;
static uint8_t sinkBuf[64];

CHI_CONSTRUCTOR(fmtTest) {
    chiSinkMemInit(&sink, sinkBuf, sizeof (sinkBuf));
}

BENCH(snprintfDouble, 100000) {
    char buf[64];
    snprintf(buf, sizeof (buf), "%f %f", 123.456, 123.456);
}

BENCH(FmtDouble, 100000) {
    char buf[64];
    chiFmt(buf, sizeof (buf), "%f %f", 123.456, 123.456);
}

BENCH(snprintfDoubleE, 100000) {
    char buf[64];
    snprintf(buf, sizeof (buf), "%e %e", 123e42, 123e-42);
}

BENCH(FmtDoubleE, 100000) {
    char buf[64];
    chiFmt(buf, sizeof (buf), "%e %e", 123e42, 123e-42);
}

BENCH(snprintfInt, 1000000) {
    char buf[64];
    snprintf(buf, sizeof (buf), "%d %d", 123, 123);
}

BENCH(FmtInt, 1000000) {
    char buf[64];
    chiFmt(buf, sizeof (buf), "%d %d", 123, 123);
}

BENCH(SinkFmtInt, 1000000) {
    sink.used = 0;
    chiSinkFmt(&sink.base, "%d %d", 123, 123);
}

BENCH(SinkInt, 1000000) {
    sink.used = 0;
    chiSinkPutu(&sink.base, 123);
    chiSinkPutc(&sink.base, ' ');
    chiSinkPutu(&sink.base, 123);
}

BENCH(SinkFmtStr, 1000000) {
    sink.used = 0;
    chiSinkFmt(&sink.base, "%S %S", chiStringRef("abc"), chiStringRef("def"));
}

BENCH(SinkPuts, 1000000) {
    sink.used = 0;
    chiSinkPuts(&sink.base, "abc");
    chiSinkPutc(&sink.base, ' ');
    chiSinkPuts(&sink.base, "def");
}

BENCH(SinkFmtChar, 1000000) {
    sink.used = 0;
    chiSinkFmt(&sink.base, "%c %c", 'a', 'b');
}

BENCH(snprintfChar, 1000000) {
    char buf[64];
    snprintf(buf, sizeof (buf), "%c %c", 'a', 'b');
}

BENCH(FmtChar, 1000000) {
    char buf[64];
    chiFmt(buf, sizeof (buf), "%c %c", 'a', 'b');
}

BENCH(SinkPutc, 1000000) {
    sink.used = 0;
    chiSinkPutc(&sink.base, 'a');
    chiSinkPutc(&sink.base, ' ');
    chiSinkPutc(&sink.base, 'b');
}

TEST(chiFmt) {
    char buf[256];

    chiFmt(buf, sizeof (buf), "%f", 0.01245);
    ASSERT(streq(buf, "0.012450"));

    chiFmt(buf, sizeof (buf), "%f", 1e45);
    ASSERT(streq(buf, "1.000000e45"));

    chiFmt(buf, sizeof (buf), "%f", 1.987654e76);
    ASSERT(streq(buf, "1.987654e76"));

    chiFmt(buf, sizeof (buf), "%e", 1.987654e-9);
    ASSERT(streq(buf, "1.987654e-9"));

    chiFmt(buf, sizeof (buf), "%f", 1.987654e-9);
    ASSERT(streq(buf, "0.000000"));

    chiFmt(buf, sizeof (buf), "%f", 1.987654e-3);
    ASSERT(streq(buf, "0.001988"));

    chiFmt(buf, sizeof (buf), "%.2f", 123.456);
    ASSERT(streq(buf, "123.46"));

    chiFmt(buf, sizeof (buf), "%f", 123.456);
    ASSERT(streq(buf, "123.456000"));

    chiFmt(buf, sizeof (buf), "%.*f", 4, 123.456);
    ASSERT(streq(buf, "123.4560"));

    chiFmt(buf, sizeof (buf), "%d", 123);
    ASSERT(streq(buf, "123"));

    chiFmt(buf, sizeof (buf), "%x", 0xabcu);
    ASSERT(streq(buf, "abc"));

    chiFmt(buf, sizeof (buf), "%5x", 0xabcu);
    ASSERT(streq(buf, "  abc"));

    chiFmt(buf, sizeof (buf), "%-5x", 0xabcu);
    ASSERT(streq(buf, "abc  "));

    chiFmt(buf, sizeof (buf), "%05x", 0xabcu);
    ASSERT(streq(buf, "00abc"));

    chiFmt(buf, sizeof (buf), "%-05x", 0xabcu);
    ASSERT(streq(buf, "abc  "));

    chiFmt(buf, sizeof (buf), "a%db", 123);
    ASSERT(streq(buf, "a123b"));

    chiFmt(buf, sizeof (buf), "a%sb", "xyz");
    ASSERT(streq(buf, "axyzb"));

    chiFmt(buf, sizeof (buf), "a%Sb", chiStringRef("xyz"));
    ASSERT(streq(buf, "axyzb"));

    chiFmt(buf, sizeof (buf), "a%6sb", "xyz");
    ASSERT(streq(buf, "a   xyzb"));

    chiFmt(buf, sizeof (buf), "a%-6sb", "xyz");
    ASSERT(streq(buf, "axyz   b"));

    chiFmt(buf, sizeof (buf), "%d %s %S", 123, "xyz", chiStringRef("XYZ"));
    ASSERT(streq(buf, "123 xyz XYZ"));
}
