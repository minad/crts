#pragma once

#include "bytes.h"

// data must be word aligned
CHI_OBJECT(BIGINT, BigInt, bool sign : 1; uint32_t size : 31; uint32_t _pad : 32; ChiWord data[])

CHI_API CHI_WU Chili chiFloat64ToBigInt(double);
CHI_API CHI_WU Chili chiBigIntFromBytes(const uint8_t*, size_t);
CHI_API CHI_WU double CHI_PRIVATE(chiSlowBigIntToFloat64)(Chili);
CHI_API CHI_WU Chili CHI_PRIVATE(chiSlowInt64ToBigInt)(int64_t);
CHI_API CHI_WU Chili CHI_PRIVATE(chiSlowUInt64ToBigInt)(uint64_t);
CHI_API CHI_WU uint64_t CHI_PRIVATE(chiSlowBigIntToUInt64)(Chili);
CHI_API CHI_WU Chili CHI_PRIVATE(chiSlowBigIntTryShl)(Chili, uint16_t);
CHI_API CHI_WU Chili CHI_PRIVATE(chiSlowBigIntShr)(Chili, uint16_t);
CHI_API CHI_WU int32_t CHI_PRIVATE(chiSlowBigIntCmp)(Chili, Chili);
CHI_API CHI_WU Chili CHI_PRIVATE(chiStaticBigInt)(const ChiStaticBytes*);

#define CHI_STATIC_BIGINT(s)                                            \
    ({                                                                  \
        static const ChiStaticBytes _staticBigInt = CHI_STATIC_BYTES(s);  \
        _Static_assert(sizeof (s) > 1, "BigInt must not be empty");      \
        _chiStaticBigInt(&_staticBigInt);                               \
    })

CHI_INL CHI_WU Chili CHI_PRIVATE(chiSmallBigIntPack)(int64_t i) {
    uint64_t x = _chiInt64Pack(i);
    CHI_ASSERT(_chiFitsUnboxed64(x));
    return _chiFromUnboxed(x);
}

CHI_INL CHI_WU int64_t CHI_PRIVATE(chiSmallBigIntUnpack)(Chili c) {
    return _chiInt64Unpack(_chiToUnboxed(c));
}

// Optimize fast path of bigint operations
#if CHI_BIGINT_SMALLOPT

CHI_API_INL CHI_WU double chiBigIntToFloat64(Chili x) {
    return CHI_LIKELY(_chiUnboxed64(x)) ? (double)_chiSmallBigIntUnpack(x) : _chiSlowBigIntToFloat64(x);
}

CHI_API_INL CHI_WU Chili chiInt64ToBigInt(int64_t x) {
    ChiWord a = _chiInt64Pack(x);
    return CHI_LIKELY(_chiFitsUnboxed64(a)) ? _chiFromUnboxed(a) : _chiSlowInt64ToBigInt(x);
}

CHI_API_INL CHI_WU Chili chiUInt64ToBigInt(uint64_t x) {
    return CHI_LIKELY(x < _CHI_INT64_SHIFT) ? _chiSmallBigIntPack((int64_t)x) : _chiSlowUInt64ToBigInt(x);
}

CHI_API_INL CHI_WU uint64_t chiBigIntToUInt64(Chili x) {
    return CHI_LIKELY(_chiUnboxed64(x)) ? (uint64_t)_chiSmallBigIntUnpack(x) : _chiSlowBigIntToUInt64(x);
}

CHI_API_INL CHI_WU Chili chiBigIntTryShl(Chili x, uint16_t y) {
    int64_t a = _chiInt64Unpack(CHILI_UN(x)), b = a << y;
    uint64_t u = _chiInt64Pack(b);
    if (CHI_LIKELY(y < 63 && (b >> y) == a && _chiFitsUnboxed64(u)))
        return _chiFromUnboxed(u);
    return _chiSlowBigIntTryShl(x, y);
}

CHI_API_INL CHI_WU Chili chiBigIntShr(Chili x, uint16_t y) {
    if (CHI_LIKELY(_chiUnboxed64(x))) {
        int64_t a = _chiSmallBigIntUnpack(x);
        return _chiSmallBigIntPack(CHI_LIKELY(y < 63) ? a >> y : (a < 0 ? -1 : 0));
    }
    return _chiSlowBigIntShr(x, y);
}

CHI_API_INL CHI_WU int32_t chiBigIntCmp(Chili x, Chili y) {
    ChiWord a = CHILI_UN(x), b = CHILI_UN(y);
    return CHI_LIKELY(_chiFitsUnboxed64(a | b))
        ? CHI_CMP(_chiInt64Unpack(a), _chiInt64Unpack(b))
        : _chiSlowBigIntCmp(x, y);
}

#define CHI_BIGINT_BINOP(f, o)                                          \
    CHI_API Chili _chiSlowBigInt##f(Chili, Chili);                      \
    CHI_API_INL CHI_WU Chili chiBigInt##f(Chili x, Chili y) {               \
        ChiWord a = CHILI_UN(x), b = CHILI_UN(y);                       \
        return CHI_LIKELY(_chiFitsUnboxed64(a | b))                     \
            ? _chiSmallBigIntPack(_chiInt64Unpack(a) o _chiInt64Unpack(b)) \
            : _chiSlowBigInt##f(x, y);                                  \
    }

#define CHI_BIGINT_BINFN(f, o)                                          \
    CHI_API Chili _chiSlowBigInt##f(Chili, Chili);                      \
    CHI_API_INL CHI_WU Chili chiBigInt##f(Chili x, Chili y) {               \
        ChiWord a = CHILI_UN(x), b = CHILI_UN(y);                       \
        return CHI_LIKELY(_chiFitsUnboxed64(a | b))                     \
            ? _chiSmallBigIntPack(o(_chiInt64Unpack(a), _chiInt64Unpack(b))) \
            : _chiSlowBigInt##f(x, y);                                  \
    }

#define CHI_BIGINT_CMP(f, o)                                    \
    CHI_API_INL CHI_WU bool chiBigInt##f(Chili x, Chili y) {        \
        ChiWord a = CHILI_UN(x), b = CHILI_UN(y);               \
        return CHI_LIKELY(_chiFitsUnboxed64(a | b))             \
            ? _chiInt64Unpack(a) o _chiInt64Unpack(b)           \
            : _chiSlowBigIntCmp(x, y) o 0;                      \
    }

#define CHI_BIGINT_TRY_BINOP(f, o)                                      \
    CHI_API Chili _chiSlowBigIntTry##f(Chili, Chili);                   \
    CHI_API_INL CHI_WU Chili chiBigIntTry##f(Chili x, Chili y) {         \
        ChiWord a = CHILI_UN(x), b = CHILI_UN(y);                       \
        int64_t c;                                                      \
        bool ovf = __builtin_##o##_overflow(_chiInt64Unpack(a), _chiInt64Unpack(b), &c); \
        ChiWord d = _chiInt64Pack(c);                                   \
        if (CHI_LIKELY(!ovf &&_chiFitsUnboxed64(a | b | d)))            \
            return _chiFromUnboxed(d);                                  \
        return _chiSlowBigIntTry##f(x, y);                              \
    }

#define CHI_BIGINT_UNOP(f, o)                                           \
    CHI_API Chili _chiSlowBigInt##f(Chili);                             \
    CHI_API_INL CHI_WU Chili chiBigInt##f(Chili x) {                    \
        ChiWord a = CHILI_UN(x), b = _chiInt64Pack(o _chiInt64Unpack(a)); \
        return CHI_LIKELY(_chiFitsUnboxed64(a | b)) ? _chiFromUnboxed(b) : _chiSlowBigInt##f(x); \
    }

#else

CHI_API_INL CHI_WU double chiBigIntToFloat64(Chili x)      { return _chiSlowBigIntToFloat64(x); }
CHI_API_INL CHI_WU Chili chiInt64ToBigInt(int64_t x)       { return _chiSlowInt64ToBigInt(x); }
CHI_API_INL CHI_WU Chili chiUInt64ToBigInt(uint64_t x)     { return _chiSlowUInt64ToBigInt(x); }
CHI_API_INL CHI_WU uint64_t chiBigIntToUInt64(Chili x)     { return _chiSlowBigIntToUInt64(x); }
CHI_API_INL CHI_WU Chili chiBigIntTryShl(Chili x, uint16_t y) { return _chiSlowBigIntTryShl(x, y); }
CHI_API_INL CHI_WU Chili chiBigIntShr(Chili x, uint16_t y) { return _chiSlowBigIntShr(x, y); }
CHI_API_INL CHI_WU int32_t chiBigIntCmp(Chili x, Chili y)  { return _chiSlowBigIntCmp(x, y); }

#define CHI_BIGINT_BINOP(f, o)                                          \
    CHI_API Chili _chiSlowBigInt##f(Chili, Chili);                      \
    CHI_API_INL CHI_WU Chili chiBigInt##f(Chili x, Chili y) { return _chiSlowBigInt##f(x, y); }

#define CHI_BIGINT_BINFN(f, o)                                          \
    CHI_API Chili _chiSlowBigInt##f(Chili, Chili);                      \
    CHI_API_INL CHI_WU Chili chiBigInt##f(Chili x, Chili y) { return _chiSlowBigInt##f(x, y); }

#define CHI_BIGINT_CMP(f, o)                                            \
    CHI_API_INL CHI_WU bool chiBigInt##f(Chili x, Chili y) { return _chiSlowBigIntCmp(x, y) o 0; }

#define CHI_BIGINT_TRY_BINOP(f, o)                                            \
    CHI_API Chili _chiSlowBigIntTry##f(Chili, Chili);                   \
    CHI_API_INL CHI_WU Chili chiBigIntTry##f(Chili x, Chili y) { return _chiSlowBigIntTry##f(x, y); }

#define CHI_BIGINT_UNOP(f, o)                                           \
    CHI_API Chili _chiSlowBigInt##f(Chili);                             \
    CHI_API_INL CHI_WU Chili chiBigInt##f(Chili x) { return _chiSlowBigInt##f(x); }

#endif

CHI_BIGINT_BINFN(Div, CHI_DIV)
CHI_BIGINT_BINFN(Mod, CHI_MOD)
CHI_BIGINT_BINFN(Rem, CHI_REM)
CHI_BIGINT_BINOP(And, &)
CHI_BIGINT_BINOP(Or, |)
CHI_BIGINT_BINOP(Quot, /)
CHI_BIGINT_BINOP(Xor, ^)
CHI_BIGINT_CMP(Eq, ==)
CHI_BIGINT_CMP(Le, <=)
CHI_BIGINT_CMP(Lt, <)
CHI_BIGINT_CMP(Ne, !=)
CHI_BIGINT_TRY_BINOP(Add, add)
CHI_BIGINT_TRY_BINOP(Mul, mul)
CHI_BIGINT_TRY_BINOP(Sub, sub)
CHI_BIGINT_UNOP(Neg, -)
CHI_BIGINT_UNOP(Not, ~)

#undef CHI_BIGINT_BINFN
#undef CHI_BIGINT_BINOP
#undef CHI_BIGINT_TRY_BINOP
#undef CHI_BIGINT_CMP
#undef CHI_BIGINT_UNOP
