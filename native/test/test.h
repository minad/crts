#pragma once

#include "../sink.h"
#include "../rand.h"

typedef void TestFn(ChiRandState, bool*);
typedef void BenchFn(ChiRandState, uint32_t);

typedef struct {
    BenchFn*    fn;
    const char* name;
    uint32_t    runs;
} Bench;

#define RAND (chiRand(_randState))

#define TEST(name)                                                      \
    static TestFn test_##name;                                          \
    static TestFn *_register_test_##name                                \
    __attribute__ ((section("test_registry"), used)) = test_##name;     \
    static void test_##name(ChiRandState _randState __attribute__ ((unused)), bool* _testFailed __attribute__ ((unused)))

#define BENCH(name, runs)                                               \
    static BenchFn bench_##name;                                        \
    static Bench _bench_##name = { bench_##name, #name, runs };         \
    static Bench* _register_bench_##name                                \
    __attribute__ ((section("bench_registry"), used)) = &_bench_##name; \
    static void bench_##name(ChiRandState _randState __attribute__ ((unused)), uint32_t benchRun __attribute__ ((unused)))

#define ASSERT(cond)                                                    \
    ({                                                                  \
        if (!(cond)) {                                                  \
            chiSinkFmt(chiStderr,                                       \
                       "%s:%d: %s: Assertion `%s' failed.\n",           \
                       __FILE__, __LINE__, __func__, #cond);            \
            *_testFailed = true;                                       \
            return;                                                     \
        }                                                               \
    })

#define _ASSERT_FMT(f)                                          \
    "%s:%d: %s: Comparison `%s %s %s' failed (" f " %s " f ").\n"

#define _ASSERT_CMP(a)                                  \
    _Generic((a),                                       \
             double:        _ASSERT_FMT("%f"),          \
             uint32_t:      _ASSERT_FMT("%u"),          \
             int32_t:       _ASSERT_FMT("%d"),          \
             uint64_t:      _ASSERT_FMT("%ju"),         \
             int64_t:       _ASSERT_FMT("%jd"),         \
             void*:         _ASSERT_FMT("%p"))

CHI_INL int64_t  _promote_int64(int64_t x)   { return x; }
CHI_INL uint64_t _promote_uint64(uint64_t x) { return x; }
CHI_INL int32_t  _promote_int32(int32_t x)   { return x; }
CHI_INL uint32_t _promote_uint32(uint32_t x) { return x; }
CHI_INL double   _promote_double(double x)   { return x; }
CHI_INL void*    _promote_ptr(void* x)       { return x; }
CHI_INL double   _promote_float(float x)     { return (double)x; }

#define _ASSERT_PROMOTE(a)                      \
    (_Generic((a),                              \
              void*:    _promote_ptr,           \
              int64_t:  _promote_int64,         \
              int32_t:  _promote_int32,         \
              int16_t:  _promote_int32,         \
              int8_t:   _promote_int32,         \
              uint64_t: _promote_uint64,        \
              uint32_t: _promote_uint32,        \
              uint16_t: _promote_uint32,        \
              uint8_t:  _promote_uint32,        \
              char:     _promote_int32,         \
              float:    _promote_float,         \
              double:   _promote_double)(a))

#define ASSERTCMP(a, cmp, b)                                            \
    ({                                                                  \
        typeof(a) _a = (a), _b = (b);                                   \
        if (!(_a cmp _b)) {                                             \
            chiSinkFmt(chiStderr,                                       \
                          _ASSERT_CMP(_ASSERT_PROMOTE(_a)),             \
                          __FILE__, __LINE__, __func__, #a, #cmp, #b,   \
                          _ASSERT_PROMOTE(_a), #cmp, _ASSERT_PROMOTE(_b)); \
            *_testFailed = true;                                        \
            return;                                                     \
        }                                                               \
    })

#define ASSERTEQ(a, b) ASSERTCMP(a, ==, b)
