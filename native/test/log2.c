#include "test.h"
#include "../num.h"
#include <math.h>

// Clay Turner's fast binary logarithm
CHI_INL float intLog2(uint64_t x) {
    uint32_t n = chiLog2(x);
    float y = (float)n, b = .5;
    float z = (float)x / (float)(1ULL << n);
    for (uint32_t i = 0; i < 5; ++i) {
        z *= z;
        if (z >= 2) {
            z /= 2;
            y += b;
        }
        b /= 2;
    }
    return y;
}

// From fastapprox by Paul Mineiro
CHI_INL float fastlog2(float x) {
    union { float f; uint32_t i; } vx = { x };
    union { uint32_t i; float f; } mx = { (vx.i & 0x7FFFFF) | 0x3F000000 };
    float y = (float)vx.i;
    y *= 1.1920928955078125e-7f;
    return y - 124.22551499f
        - 1.498030302f * mx.f
        - 1.72587999f / (.3520887068f + mx.f);
}

BENCH(log2, 1000000) {
    __volatile__ double x = log2((double)benchRun + 1);
    CHI_NOWARN_UNUSED(x);
}

BENCH(intLog2, 1000000) {
    __volatile__ float x = intLog2(benchRun + 1);
    CHI_NOWARN_UNUSED(x);
}

BENCH(chiLog2, 1000000) {
    __volatile__ uint32_t x = chiLog2(benchRun + 1);
    CHI_NOWARN_UNUSED(x);
}

BENCH(fastlog2, 1000000) {
    __volatile__ float x = fastlog2((float)benchRun + 1);
    CHI_NOWARN_UNUSED(x);
}

/*
TEST(log2) {
    CHI_AUTO_SINK(s, chiSinkFileTryNew("log2.txt", CHI_SINK_BINARY));
    ASSERT(s);
    for (uint64_t i = 1; i < 1000; ++i)
        chiSinkFmt(s, "%ju %f %f %f\n", i, log2((double)i), (double)intLog2(i), (double)fastlog2((float)i));
}
*/
