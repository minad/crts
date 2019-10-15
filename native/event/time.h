#pragma once

#include "defs.h"

#define CHI_TIME_OPS(T, f)                                              \
    CHI_INL Chi##T chiNanosTo##T(ChiNanos a)   { return chi##T(CHI_UN(Nanos, a) / f); } \
    CHI_INL Chi##T chiMicrosTo##T(ChiMicros a) { return chi##T(f > 1000 ? CHI_UN(Micros, a) / (f / 1000) : CHI_UN(Micros, a) * (1000 / f)); } \
    CHI_INL Chi##T chiMillisTo##T(ChiMillis a) { return chi##T(f > 1000000 ? CHI_UN(Millis, a) / (f / 1000000) : CHI_UN(Millis, a) * (1000000 / f)); } \
    CHI_INL Chi##T chi##T##Add(Chi##T a,  Chi##T b) { return chi##T(CHI_UN(T, a) + CHI_UN(T, b)); } \
    CHI_INL Chi##T chi##T##Delta(Chi##T a, Chi##T b) { return chi##T(CHI_UN(T, a) > CHI_UN(T, b) ? CHI_UN(T, a) - CHI_UN(T, b) : 0); } \
    CHI_INL Chi##T chi##T##Max(Chi##T a,  Chi##T b) { return chi##T(CHI_MAX(CHI_UN(T, a), CHI_UN(T, b))); } \
    CHI_INL Chi##T chi##T##Min(Chi##T a,  Chi##T b) { return chi##T(CHI_MIN(CHI_UN(T, a), CHI_UN(T, b))); } \
    CHI_INL bool   chi##T##Less(Chi##T a, Chi##T b) { return CHI_UN(T, a) < CHI_UN(T, b); } \
    CHI_INL bool   chi##T##Eq(Chi##T a, Chi##T b)   { return CHI_UN(T, a) == CHI_UN(T, b); } \
    CHI_INL bool   chi##T##Zero(Chi##T a)           { return !CHI_UN(T, a); }
#define chiMillis(a) CHI_WRAP(Millis, (a))
#define chiMicros(a) CHI_WRAP(Micros, (a))
#define chiNanos(a)  CHI_WRAP(Nanos, (a))
CHI_TIME_OPS(Millis, 1000000)
CHI_TIME_OPS(Micros, 1000)
CHI_TIME_OPS(Nanos, 1)
#undef CHI_TIME_OPS

CHI_INL CHI_WU double chiNanosRatio(ChiNanos a, ChiNanos b) {
    double c = (double)CHI_UN(Nanos, b);
    return c != 0. ? (double)CHI_UN(Nanos, a) / c : 1;
}

CHI_INL CHI_WU double chiPerSec(uint64_t a, ChiNanos b) {
    double c = (double)CHI_UN(Nanos, b);
    return c != 0. ? (double)a / (c / 1e9) : 0.;
}
