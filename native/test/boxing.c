#include <float.h>
#include "test.h"

enum { SAMPLES = 10000 };

extern uint64_t unboxing_bench;
uint64_t unboxing_bench = 0;

BENCH(unboxed_large, 20000000) {
    uint64_t x = BENCHRUN * UINT64_C(123225453243242);
    unboxing_bench += x <= UINT64_C(0xfff0000000000000);
}

BENCH(unboxed_small, 20000000) {
    uint64_t x = BENCHRUN * UINT64_C(123225453243242);
    unboxing_bench += (x >> 51) != 0x1FFF;
}

TEST(Wrap) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        void* p = (ChiWord*)CHI_CHUNK_START + RAND % (CHI_CHUNK_MAX_SIZE / CHI_WORDSIZE);
        size_t s = RAND % _CHILI_LARGE + 1;
        ChiType t = (ChiType)(RAND % ((uint32_t)CHI_LAST_TYPE + 1));
        ChiGen g = (ChiGen)(RAND % ((uint32_t)CHI_GEN_MAJOR + 1));
        Chili c = chiWrap(p, s, t, g);
        ASSERT(chiType(c) == t);
        ASSERT(chiGen(c) == g);
        ASSERT(_chiSizeField(c) == s);
        ASSERT(_chiPtrField(c) == p);
        ASSERT(!chiEmpty(c));
    }
}

TEST(Bool) {
    ASSERT(chiTrue(chiFromBool(true)));
    ASSERT(!chiTrue(chiFromBool(false)));
    ASSERT(chiTrue(chiFromBool(42)));
    ASSERT(!chiTrue(chiFromBool(0)));

    ASSERT(chiToBool(chiFromBool(true)));
    ASSERT(!chiToBool(chiFromBool(false)));
    ASSERT(chiToBool(chiFromBool(42)));
    ASSERT(!chiToBool(chiFromBool(0)));

    ASSERT(!chiRef(chiFromBool(false)));
    ASSERT(!chiRef(chiFromBool(true)));
    ASSERT(!chiRef(chiFromBool(0)));
    ASSERT(!chiRef(chiFromBool(42)));

    ASSERT(chiTrue(chiWrap((void*)CHI_CHUNK_START, 1, CHI_PRESTRING, CHI_GEN_NURSERY)));
    ASSERT(chiTrue(chiWrap((void*)CHI_CHUNK_START, 1, CHI_PRESTRING, CHI_GEN_NURSERY)) == true);
}

TEST(Poison) {
    ASSERT(chiRef(_CHILI_POISON));
    ASSERT(chiEmpty(_CHILI_POISON));
    ASSERT(chiType(_CHILI_POISON) == CHI_POISON_TAG);
}

TEST(Empty) {
    ASSERT(!chiRef(chiNewEmpty(CHI_TAG(0))));
    ASSERT(chiEmpty(chiNewEmpty(CHI_TAG(0))));
}

TEST(Int8) {
    for (int64_t i = INT8_MIN; i <= INT8_MAX; ++i) {
        int8_t x = (int8_t)i;
        ASSERT(!chiRef(chiFromInt8(x)));
        ASSERTEQ(chiToInt8(chiFromInt8(x)), x);
    }
}

TEST(UInt8) {
    for (uint64_t i = 0; i <= UINT8_MAX; ++i) {
        uint8_t x = (uint8_t)i;
        ASSERT(!chiRef(chiFromUInt8(x)));
        ASSERTEQ(chiToUInt8(chiFromUInt8(x)), x);
    }
}

TEST(Int16) {
    for (int64_t i = INT16_MIN; i <= INT16_MAX; ++i) {
        int16_t x = (int16_t)i;
        ASSERT(!chiRef(chiFromInt16(x)));
        ASSERTEQ(chiToInt16(chiFromInt16(x)), x);
    }
}

TEST(UInt16) {
    for (uint64_t i = 0; i <= UINT16_MAX; ++i) {
        uint16_t x = (uint16_t)i;
        ASSERT(!chiRef(chiFromUInt16(x)));
        ASSERTEQ(chiToUInt16(chiFromUInt16(x)), x);
    }
}

TEST(Int32) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        int32_t x = (int32_t)RAND;
        ASSERT(!chiRef(chiFromInt32(x)));
        ASSERTEQ(chiToInt32(chiFromInt32(x)), x);
    }
}

TEST(UInt32) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        uint32_t x = (uint32_t)RAND;
        ASSERT(!chiRef(chiFromUInt32(x)));
        ASSERTEQ(chiToUInt32(chiFromUInt32(x)), x);
    }
}

TEST(Int64) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        int64_t x = (int64_t)RAND;
        ASSERTEQ(chiToInt64(chiFromInt64(x)), x);
    }
    ASSERT(!chiRef(chiFromInt64(0)));
    ASSERT(!chiRef(chiFromInt64(1)));
    ASSERT(!chiRef(chiFromInt64(-1)));
    ASSERT(!chiRef(chiFromInt64((INT64_C(1) << 62) - 1)));
    ASSERT(chiType(chiFromInt64(INT64_C(1) << 62)) == CHI_BOX);
    ASSERT(!chiRef(chiFromInt64(-(INT64_C(1) << 62))));
    ASSERT(chiType(chiFromInt64(-(INT64_C(1) << 62) - 1)) == CHI_BOX);
}

TEST(UInt64) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        uint64_t x = RAND;
        ASSERTEQ(chiToUInt64(chiFromUInt64(x)), x);
    }
    ASSERT(!chiRef(chiFromUInt64(0)));
    ASSERT(!chiRef(chiFromUInt64(42)));
    ASSERT(!chiRef(chiFromUInt64((UINT64_C(1) << 63) - 1U)));
    ASSERT(chiType(chiFromUInt64(~UINT64_C(1))) == CHI_BOX);
    ASSERT(chiType(chiFromUInt64(UINT64_C(1) << 63)) == CHI_BOX);
}

#define CHECK_FLOAT32(x)                                                \
    ({                                                                  \
        float _x = (x);                                                 \
        ASSERT(!chiRef(chiFromFloat32(_x)));                     \
        if (_x != _x) {                                                 \
            float y = chiToFloat32(chiFromFloat32(_x));             \
            ASSERT(y != y);                                             \
        } else {                                                        \
            ASSERTEQ(chiToFloat32(chiFromFloat32(_x)), _x); \
        }                                                               \
    })

TEST(Float32) {
    for (uint32_t i = 0; i < SAMPLES; ++i)
        CHECK_FLOAT32(FLT_MAX * ((float)RAND / (float)INT64_MAX));
    CHECK_FLOAT32(1.0f / 0.0f);
    CHECK_FLOAT32(-1.0f / 0.0f);
    CHECK_FLOAT32(0.0f / 0.0f);
}

#define CHECK_FLOAT64(x)                                                \
    ({                                                                  \
        double _x = (x);                                                \
        ASSERT(CHI_NANBOXING_ENABLED == !chiRef(chiFromFloat64(_x))); \
        if (_x != _x) {                                                 \
            double y = chiToFloat64(chiFromFloat64(_x));                \
            ASSERT(y != y);                                             \
        } else {                                                        \
            ASSERTEQ(chiToFloat64(chiFromFloat64(_x)), _x);             \
        }                                                               \
    })

TEST(Float64) {
    for (uint32_t i = 0; i < SAMPLES; ++i)
        CHECK_FLOAT64(DBL_MAX * ((double)RAND / (double)INT64_MAX));
    CHECK_FLOAT64(1.0 / 0.0);
    CHECK_FLOAT64(-1.0 / 0.0);
    CHECK_FLOAT64(0.0 / 0.0);
}

TEST(chiFloat64ToInt64) {
    ASSERTEQ(chiFloat64ToInt64(0), 0);
    ASSERTEQ(chiFloat64ToInt64(1), 1);
    ASSERTEQ(chiFloat64ToInt64(-1), -1);
    ASSERTEQ(chiFloat64ToInt64(42), 42);
    ASSERTEQ(chiFloat64ToInt64(-42), -42);
    ASSERTEQ(chiFloat64ToInt64(0./0.), 0);
    ASSERTEQ(chiFloat64ToInt64(1./0.), 0);
    ASSERTEQ(chiFloat64ToInt64(-1./0.), 0);
    ASSERTEQ(chiFloat64ToInt64(9999999.0), 9999999);
}
