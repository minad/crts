#include <chili/object.h>
#include "test.h"
#include <float.h>

enum { SAMPLES = 10000 };

TEST(Bool) {
    ASSERT(chiTrue(chiFromBool(true)));
    ASSERT(!chiTrue(chiFromBool(false)));
    ASSERT(chiTrue(chiFromBool(42)));
    ASSERT(!chiTrue(chiFromBool(0)));

    ASSERT(_chiUnboxed(chiFromBool(false)));
    ASSERT(_chiUnboxed(chiFromBool(true)));
    ASSERT(_chiUnboxed(chiFromBool(0)));
    ASSERT(_chiUnboxed(chiFromBool(42)));

    ASSERT(chiTrue(chiWrap((void*)CHI_CHUNK_START, 0, CHI_RAW)));
    ASSERT(chiTrue(chiWrap((void*)CHI_CHUNK_START, 0, CHI_RAW)) == true);
}

TEST(Int8) {
    for (int64_t i = INT8_MIN; i <= INT8_MAX; ++i) {
        int8_t x = (int8_t)i;
        ASSERT(_chiUnboxed(chiFromInt8(x)));
        ASSERTEQ(chiToInt8(chiFromInt8(x)), x);
    }
}

TEST(UInt8) {
    for (uint64_t i = 0; i <= UINT8_MAX; ++i) {
        uint8_t x = (uint8_t)i;
        ASSERT(_chiUnboxed(chiFromUInt8(x)));
        ASSERTEQ(chiToUInt8(chiFromUInt8(x)), x);
    }
}

TEST(Int16) {
    for (int64_t i = INT16_MIN; i <= INT16_MAX; ++i) {
        int16_t x = (int16_t)i;
        ASSERT(_chiUnboxed(chiFromInt16(x)));
        ASSERTEQ(chiToInt16(chiFromInt16(x)), x);
    }
}

TEST(UInt16) {
    for (uint64_t i = 0; i <= UINT16_MAX; ++i) {
        uint16_t x = (uint16_t)i;
        ASSERT(_chiUnboxed(chiFromUInt16(x)));
        ASSERTEQ(chiToUInt16(chiFromUInt16(x)), x);
    }
}

TEST(Int32) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        int32_t x = (int32_t)RAND;
        ASSERT(_chiUnboxed(chiFromInt32(x)));
        ASSERTEQ(chiToInt32(chiFromInt32(x)), x);
    }
}

TEST(UInt32) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        uint32_t x = (uint32_t)RAND;
        ASSERT(_chiUnboxed(chiFromUInt32(x)));
        ASSERTEQ(chiToUInt32(chiFromUInt32(x)), x);
    }
}

TEST(Int64) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        int64_t x = (int64_t)RAND;
        ASSERTEQ(chiToInt64(chiFromInt64(x)), x);
    }
    ASSERT(_chiUnboxed(chiFromInt64(0)));
    ASSERT(_chiUnboxed(chiFromInt64(1)));
    ASSERT(_chiUnboxed(chiFromInt64(-1)));
    ASSERT(_chiUnboxed(chiFromInt64((1LL << 62) - 1)));
    ASSERT(_chiType(chiFromInt64(1LL << 62)) == CHI_BOX64);
    ASSERT(_chiUnboxed(chiFromInt64(-(1LL << 62))));
    ASSERT(_chiType(chiFromInt64(-(1LL << 62) - 1)) == CHI_BOX64);
}

TEST(UInt64) {
    for (uint32_t i = 0; i < SAMPLES; ++i) {
        uint64_t x = RAND;
        ASSERTEQ(chiToUInt64(chiFromUInt64(x)), x);
    }
    ASSERT(_chiUnboxed(chiFromUInt64(0)));
    ASSERT(_chiUnboxed(chiFromUInt64(42)));
    ASSERT(_chiUnboxed(chiFromUInt64((1ULL << 63) - 1ULL)));
    ASSERT(_chiType(chiFromUInt64(~1ULL)) == CHI_BOX64);
    ASSERT(_chiType(chiFromUInt64(1ULL << 63)) == CHI_BOX64);
}

#define CHECK_FLOAT32(x)                                                \
    ({                                                                  \
        float _x = (x);                                                 \
        ASSERT(_chiUnboxed(chiFromFloat32(_x)));                     \
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
}

#define CHECK_FLOAT64(x)                                                \
    ({                                                                  \
        double _x = (x);                                                \
        ASSERT(CHI_NANBOXING_ENABLED == _chiUnboxed(chiFromFloat64(_x))); \
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
