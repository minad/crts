#include "error.h"
#include "event.h"
#include "runtime.h"

#define Z_GMP       CHI_BIGINT_GMP
#if CHI_ARCH_32BIT
#  define Z_BITS    32
#else
#  define Z_BITS    64
#endif
#define Z_FREE(x)   ({})
#define Z_SCRATCH   64
#define Z_ASSERT(x) CHI_ASSERT(x)
#define Z_ALLOC(s)                                                      \
    ({                                                                  \
        size_t _s = (s);                                                \
        void* _r = 0;                                                   \
        if (CHI_LIKELY(_s <= CHI_BIGINT_LIMIT)) {                       \
            Chili _c = chiNewFlags(CHI_BIGINT_POS, CHI_BYTES_TO_WORDS(_s), CHI_NEW_TRY); \
            _r = chiSuccess(_c) ? chiRawPayload(_c) : 0;                 \
        }                                                               \
        _r;                                                             \
    })
#include "bigint/z.h"

static z_digit* chi_get_digit(Chili c) {
    CHI_ASSERT(chiType(c) == CHI_BIGINT_POS || chiType(c) == CHI_BIGINT_NEG);
    return (z_digit*)chiRawPayload(c);
}

static Chili chi_try_from_z(z_int z) {
    if (CHI_UNLIKELY(z.err)) {
        ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
        chiEvent0(proc, BIGINT_OVERFLOW);
        return proc->rt->fail;
    }

    if (!z.size) // Zero
        return CHI_BIGINT_UNBOXING ? CHI_FALSE : chiNewEmpty(CHI_BIGINT_POS);

    if (CHI_BIGINT_UNBOXING && z.size == 1 && !((uint64_t)z.d[0] >> 62))
        return chiInt64Pack(z.neg ? -(int64_t)z.d[0] : (int64_t)z.d[0]);

    size_t allocW = CHI_BYTES_TO_WORDS((size_t)z.alloc * sizeof (z_digit)),
        sizeW = CHI_BYTES_TO_WORDS((size_t)z.size * sizeof (z_digit));
    ChiType t = z.neg ? CHI_BIGINT_NEG : CHI_BIGINT_POS;

    if (allocW <= CHI_MAX_UNPINNED)
        return chiWrap(z.d, sizeW, t, CHI_GEN_NURSERY);

    Chili c = chiWrapLarge(z.d, sizeW, t);
    if (sizeW < allocW)
        chiObjectSetSize(chiObject(c), sizeW);
    return c;
}

static Chili chi_from_z(z_int z) {
    Chili c = chi_try_from_z(z);
    if (!chiSuccess(c))
        chiErr("BigInt operation failed");
    return c;
}

static z_int chi_to_z(Chili c) {
    if (CHI_BIGINT_UNBOXING && chiUnboxed63(c))
        return z_from_i64(chiInt64Unpack(c));
    if (!CHI_BIGINT_UNBOXING && chiEmpty(c))
        return z_zero;

    z_digit* digit = chi_get_digit(c);
    z_size size = (z_size)(chiSize(c) * CHI_WORDSIZE / sizeof (z_digit));

    /* Adjust size if top digits are unused.
     * This happens only for 32 bit and smaller digits,
     * since then multiple digits fit into one ChiWord.
     */
    if (sizeof (z_digit) < CHI_WORDSIZE) {
        for (size_t i = 0; i < CHI_WORDSIZE / sizeof (z_digit)
             && size && !digit[size - 1]; ++i, --size);
    }

    return (z_int){.d = digit, .neg = chiType(c) == CHI_BIGINT_NEG,
            .size = size, .alloc = size };
}

Chili chiStaticBigInt(const ChiStaticBytes* b) {
    return chiBigIntFromBytes(b->bytes, b->size);
}

Chili chiBigIntFromBytes(const uint8_t* buf, uint32_t size) {
    if (CHI_BIGINT_UNBOXING && size < 8) {
        int64_t x = 0;
        for (uint32_t i = 0; i < size; ++i)
            x = (x << 8) | (int64_t)buf[i];
        return chiInt64Pack(x);
    }
    return chi_from_z(z_from_b(buf, size));
}

Chili chiFloat64ToBigInt(double x) {
    return chi_from_z(z_from_d(x));
}

Chili chiSlowInt64ToBigInt(int64_t i) {
    return chi_from_z(z_from_i64(i));
}

Chili chiSlowUInt64ToBigInt(uint64_t i) {
    return chi_from_z(z_from_u64(i));
}

int32_t chiSlowBigIntCmp(Chili a, Chili b) {
    return z_cmp(chi_to_z(a), chi_to_z(b));
}

double chiSlowBigIntToFloat64(Chili c) {
    return z_to_d(chi_to_z(c));
}

uint64_t chiSlowBigIntToUInt64(Chili c) {
    return z_to_u64(chi_to_z(c));
}

Chili chiSlowBigIntTryShr(Chili a, uint16_t b) {
    return chi_try_from_z(z_shr(chi_to_z(a), b));
}

Chili chiSlowBigIntTryShl(Chili a, uint16_t b) {
    return chi_try_from_z(z_shl(chi_to_z(a), b));
}

Chili _chiSlowBigIntTryNeg(Chili c) {
    if (!CHI_BIGINT_UNBOXING && chiEmpty(c)) // zero
        return c;
    size_t size = chiSize(c);
    /* We might consider only changing the type of `c`.
     * However this would break object identity invariant
     * in the scavenger forwarding code.
     */
    Chili d = chiNewFlags(chiType(c) == CHI_BIGINT_POS ? CHI_BIGINT_NEG : CHI_BIGINT_POS, size, CHI_NEW_TRY);
    if (chiSuccess(d))
        memcpy(chi_get_digit(d), chi_get_digit(c), size * CHI_WORDSIZE);
    return d;
}

Chili _chiSlowBigIntTryNot(Chili a) {
    return chi_try_from_z(z_not(chi_to_z(a)));
}

#define CHI_BIGINT_TRY_BINOP(Op, op)                                    \
    Chili _chiSlowBigIntTry##Op(Chili a, Chili b) {                     \
        return chi_try_from_z(op(chi_to_z(a), chi_to_z(b)));        \
    }
CHI_BIGINT_TRY_BINOP(Sub, z_sub)
CHI_BIGINT_TRY_BINOP(Add, z_add)
CHI_BIGINT_TRY_BINOP(Mul, z_mul)
CHI_BIGINT_TRY_BINOP(Div, z_div)
CHI_BIGINT_TRY_BINOP(Mod, z_mod)
CHI_BIGINT_TRY_BINOP(Quo, z_quo)
CHI_BIGINT_TRY_BINOP(And, z_and)
CHI_BIGINT_TRY_BINOP(Or,  z_or)
CHI_BIGINT_TRY_BINOP(Xor, z_xor)
CHI_BIGINT_TRY_BINOP(Rem, z_rem)
#undef CHI_BIGINT_TRY_BINOP
