CHI_EXPORT CHI_WU Chili chiFloat64ToBigInt(double);
CHI_EXPORT CHI_WU Chili chiBigIntFromBytes(const uint8_t*, uint32_t);
CHI_EXPORT CHI_WU double CHI_PRIVATE(chiSlowBigIntToFloat64)(Chili);
CHI_EXPORT CHI_WU Chili CHI_PRIVATE(chiSlowInt64ToBigInt)(int64_t);
CHI_EXPORT CHI_WU Chili CHI_PRIVATE(chiSlowUInt64ToBigInt)(uint64_t);
CHI_EXPORT CHI_WU uint64_t CHI_PRIVATE(chiSlowBigIntToUInt64)(Chili);
CHI_EXPORT CHI_WU Chili CHI_PRIVATE(chiSlowBigIntTryShl)(Chili, uint16_t);
CHI_EXPORT CHI_WU Chili CHI_PRIVATE(chiSlowBigIntTryShr)(Chili, uint16_t);
CHI_EXPORT CHI_WU int32_t CHI_PRIVATE(chiSlowBigIntCmp)(Chili, Chili);
CHI_EXPORT CHI_WU Chili CHI_PRIVATE(chiStaticBigInt)(const ChiStaticBytes*);

#define CHI_STATIC_BIGINT(s)                                            \
    ({                                                                  \
        static const ChiStaticBytes _staticBigInt = CHI_STATIC_BYTES(s); \
        _Static_assert(CHI_STRSIZE(s) > 0, "BigInt must not be empty"); \
        _chiStaticBigInt(&_staticBigInt);                               \
    })

CHI_INL CHI_WU double chiBigIntToFloat64(Chili x) {
    if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiUnboxed63(x)))
        return (double)_chiInt64Unpack(x);
    return _chiSlowBigIntToFloat64(x);
}

CHI_INL CHI_WU Chili chiInt64ToBigInt(int64_t x) {
    if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiInt64Unboxed(x)))
        return _chiInt64Pack(x);
    return _chiSlowInt64ToBigInt(x);
}

CHI_INL CHI_WU Chili chiUInt64ToBigInt(uint64_t x) {
    if (CHI_BIGINT_UNBOXING && CHI_LIKELY(!(x >> 62)))
        return _chiInt64Pack((int64_t)x);
    return _chiSlowUInt64ToBigInt(x);
}

CHI_INL CHI_WU uint64_t chiBigIntToUInt64(Chili x) {
    if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiUnboxed63(x)))
        return (uint64_t)_chiInt64Unpack(x);
    return _chiSlowBigIntToUInt64(x);
}

CHI_INL CHI_WU Chili chiBigIntTryShl(Chili x, uint16_t y) {
    if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiUnboxed63(x) && y < 63)) {
        int64_t a = _chiInt64Unpack(x), b = a << y;
        if (CHI_LIKELY((b >> y) == a))
            return _chiInt64Pack(b);
    }
    return _chiSlowBigIntTryShl(x, y);
}

CHI_INL CHI_WU Chili chiBigIntTryShr(Chili x, uint16_t y) {
    if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiUnboxed63(x))) {
        int64_t a = _chiInt64Unpack(x);
        return _chiInt64Pack(CHI_LIKELY(y < 63) ? a >> y : (a < 0 ? -1 : 0));
    }
    return _chiSlowBigIntTryShr(x, y);
}

CHI_INL CHI_WU int32_t chiBigIntCmp(Chili x, Chili y) {
    if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiFitsUnboxed63(CHILI_UN(x) | CHILI_UN(y))))
        return CHI_CMP(_chiInt64Unpack(x), _chiInt64Unpack(y));
    return _chiSlowBigIntCmp(x, y);
}

#define CHI_BIGINT_TRY_BINOP(f, o)                                      \
    CHI_EXPORT Chili _chiSlowBigIntTry##f(Chili, Chili);                \
    CHI_INL CHI_WU Chili chiBigIntTry##f(Chili x, Chili y) {            \
        if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiFitsUnboxed63(CHILI_UN(x) | CHILI_UN(y)))) \
            return _chiInt64Pack(_chiInt64Unpack(x) o _chiInt64Unpack(y)); \
        return  _chiSlowBigIntTry##f(x, y);                             \
    }

#define CHI_BIGINT_TRY_BINFN(f, o)                                      \
    CHI_EXPORT Chili _chiSlowBigIntTry##f(Chili, Chili);                \
    CHI_INL CHI_WU Chili chiBigIntTry##f(Chili x, Chili y) {            \
        if (CHI_BIGINT_UNBOXING && CHI_LIKELY(_chiFitsUnboxed63(CHILI_UN(x) | CHILI_UN(y)))) \
            return _chiInt64Pack(o(_chiInt64Unpack(x), _chiInt64Unpack(y))); \
        return _chiSlowBigIntTry##f(x, y);                              \
    }

#define CHI_BIGINT_CMP(f, o)                                        \
    CHI_INL CHI_WU bool chiBigInt##f(Chili x, Chili y) {            \
        return chiBigIntCmp(x, y) o 0;                              \
    }

#define CHI_BIGINT_OVF_BINOP(f, o)                                      \
    CHI_EXPORT Chili _chiSlowBigIntTry##f(Chili, Chili);                   \
    CHI_INL CHI_WU Chili chiBigIntTry##f(Chili x, Chili y) {            \
        int64_t z;                                                      \
        if (CHI_BIGINT_UNBOXING &&                                      \
            CHI_LIKELY(_chiFitsUnboxed63(CHILI_UN(x) | CHILI_UN(y)) &&  \
                       !__builtin_##o##_overflow(_chiInt64Unpack(x), _chiInt64Unpack(y), &z) && \
                       _chiInt64Unboxed(z)))                            \
            return _chiInt64Pack(z);                                    \
        return _chiSlowBigIntTry##f(x, y);                              \
    }

#define CHI_BIGINT_TRY_UNOP(f, o)                                       \
    CHI_EXPORT Chili _chiSlowBigIntTry##f(Chili);                       \
    CHI_INL CHI_WU Chili chiBigIntTry##f(Chili x) {                     \
        int64_t y;                                                      \
        if (CHI_BIGINT_UNBOXING &&                                      \
            CHI_LIKELY(_chiUnboxed63(x) && _chiInt64Unboxed(y = o _chiInt64Unpack(x)))) \
            return _chiInt64Pack(y);                                    \
        return _chiSlowBigIntTry##f(x);                                 \
    }

CHI_BIGINT_CMP(Eq, ==)
CHI_BIGINT_CMP(Le, <=)
CHI_BIGINT_CMP(Lt, <)
CHI_BIGINT_CMP(Ne, !=)
CHI_BIGINT_OVF_BINOP(Add, add)
CHI_BIGINT_OVF_BINOP(Mul, mul)
CHI_BIGINT_OVF_BINOP(Sub, sub)
CHI_BIGINT_TRY_BINFN(Div, CHI_DIV)
CHI_BIGINT_TRY_BINFN(Mod, CHI_MOD)
CHI_BIGINT_TRY_BINFN(Rem, CHI_REM)
CHI_BIGINT_TRY_BINOP(And, &)
CHI_BIGINT_TRY_BINOP(Or, |)
CHI_BIGINT_TRY_BINOP(Quo, /)
CHI_BIGINT_TRY_BINOP(Xor, ^)
CHI_BIGINT_TRY_UNOP(Neg, -)
CHI_BIGINT_TRY_UNOP(Not, ~)

#undef CHI_BIGINT_CMP
#undef CHI_BIGINT_OVF_BINOP
#undef CHI_BIGINT_TRY_BINFN
#undef CHI_BIGINT_TRY_BINOP
#undef CHI_BIGINT_TRY_UNOP
