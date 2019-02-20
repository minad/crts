#include "bigint.h"
#include "event.h"
#include "runtime.h"
#include "mem.h"

#define BIGINT_HEADER 1

static void* bigIntAlloc(size_t size) {
    return (ChiWord*)chiRawPayload(chiNew(CHI_BIGINT, BIGINT_HEADER + CHI_BYTES_TO_WORDS(size))) + BIGINT_HEADER;
}

static void* bigIntRealloc(void* p, size_t oldSize, size_t newSize) {
    return newSize > oldSize ? memcpy(bigIntAlloc(newSize), p, oldSize) : p;
}

static void bigIntFree(void* CHI_UNUSED(p), size_t CHI_UNUSED(s)) {
    // Do nothing, memory is handled by the GC
}

#include CHI_INCLUDE(bigint/CHI_BIGINT_BACKEND)

static Chili fromBigInt(const BigInt z) {
    bool sign;
    size_t size, alloc;
    void* data;
    bigIntUnpack(z, &sign, &size, &alloc, &data);

    if (CHI_BIGINT_SMALLOPT && size < CHI_WORDSIZE)
        return chiSmallBigIntPack((int64_t)bigIntToU(z));

    if (!CHI_BIGINT_SMALLOPT && !size) // Zero
        return chiNewEmpty(CHI_BIGINT);

    ChiBigInt* bi = CHI_ALIGN_CAST32((ChiWord*)data - BIGINT_HEADER, ChiBigInt*);
    bi->sign = sign;
    bi->size = size & INT32_MAX;

    size_t allocW = CHI_BYTES_TO_WORDS(alloc) + BIGINT_HEADER,
        sizeW = CHI_BYTES_TO_WORDS(size) + BIGINT_HEADER;

    if (allocW <= CHI_MAX_UNPINNED)
        return chiWrap(bi, sizeW, CHI_BIGINT);

    Chili c = chiWrap(bi, _CHILI_PINNED, CHI_BIGINT);
    if (sizeW < allocW)
        chiObjectShrink(chiObject(c), allocW, sizeW);
    return c;
}

static Chili tryFromBigInt(const BigInt z) {
    Chili c = fromBigInt(z);
    if (CHI_LIKELY(chiUnboxed(c) || chiSize(c) * CHI_WORDSIZE < CHI_BIGINT_LIMIT))
        return c;
    CHI_EVENT0(CHI_CURRENT_PROCESSOR, BIGINT_OVERFLOW);
    return CHI_FAIL;
}

static const BigIntStruct* toBigInt(Chili c, BigInt z) {
    if (CHI_BIGINT_SMALLOPT && chiUnboxed64(c)) {
        bigIntInitI(z, chiSmallBigIntUnpack(c));
    } else if (!CHI_BIGINT_SMALLOPT && chiEmpty(c)) {
        bigIntInit(z); // Zero
    } else {
        ChiBigInt* bi = chiToBigInt(c);
        bigIntPack(z, bi->sign, bi->size, bi->data);
    }
    return z;
}

Chili chiStaticBigInt(const ChiStaticBytes* b) {
    return chiBigIntFromBytes(b->bytes, b->size);
}

Chili chiBigIntFromBytes(const uint8_t* s, size_t size) {
    if (CHI_BIGINT_SMALLOPT) {
        switch (size) {
        case 1: return chiSmallBigIntPack(s[0]);
        case 2: return chiSmallBigIntPack(((int64_t)s[0] <<  8) |  (int64_t)s[1]);
        case 3: return chiSmallBigIntPack(((int64_t)s[0] << 16) | ((int64_t)s[1] << 8)  |  (int64_t)s[2]);
        case 4: return chiSmallBigIntPack(((int64_t)s[0] << 24) | ((int64_t)s[1] << 16) | ((int64_t)s[2] <<  8) |  (int64_t)s[3]);
        case 5: return chiSmallBigIntPack(((int64_t)s[0] << 32) | ((int64_t)s[1] << 24) | ((int64_t)s[2] << 16) | ((int64_t)s[3] <<  8) |  (int64_t)s[4]);
        case 6: return chiSmallBigIntPack(((int64_t)s[0] << 40) | ((int64_t)s[1] << 32) | ((int64_t)s[2] << 24) | ((int64_t)s[3] << 16) | ((int64_t)s[4] <<  8) | (int64_t)s[5]);
        case 7: return chiSmallBigIntPack(((int64_t)s[0] << 48) | ((int64_t)s[1] << 40) | ((int64_t)s[2] << 32) | ((int64_t)s[3] << 24) | ((int64_t)s[4] << 16) | ((int64_t)s[5] << 8) | (int64_t)s[6]);
        }
    }
    BigInt z;
    bigIntInitB(z, s, size);
    return fromBigInt(z);
}

Chili chiFloat64ToBigInt(double x) {
    BigInt z;
    bigIntInitF(z, x);
    return fromBigInt(z);
}

Chili chiSlowInt64ToBigInt(int64_t i) {
    BigInt z;
    bigIntInitI(z, i);
    return fromBigInt(z);
}

Chili chiSlowUInt64ToBigInt(uint64_t i) {
    BigInt z;
    bigIntInitU(z, i);
    return fromBigInt(z);
}

int32_t chiSlowBigIntCmp(Chili a, Chili b) {
    BigInt ta, tb;
    return bigIntCmp(toBigInt(a, ta), toBigInt(b, tb));
}

double chiSlowBigIntToFloat64(Chili c) {
    BigInt t;
    return bigIntToF(toBigInt(c, t));
}

uint64_t chiSlowBigIntToUInt64(Chili c) {
    BigInt t;
    return bigIntToU(toBigInt(c, t));
}

Chili chiSlowBigIntShr(Chili a, uint16_t b) {
    BigInt z, t;
    bigIntInit(z);
    bigIntShr(z, toBigInt(a, t), b);
    return fromBigInt(z);
}

Chili chiSlowBigIntTryShl(Chili a, uint16_t b) {
    BigInt z, t;
    bigIntInit(z);
    bigIntShl(z, toBigInt(a, t), b);
    return tryFromBigInt(z);
}

const char* chiBigIntStr(Chili c, ChiBigIntBuf* buf) {
    BigInt z;
    toBigInt(c, z);
    size_t n = bigIntStrSize(z);
    if (buf->size < n) {
        buf->buf = (char*)chiRealloc(buf->buf, n);
        buf->size = n;
    }
    return bigIntStr(z, buf->buf);
}

#define CHI_BIGINT_BINOP(op)                                    \
    Chili _chiSlowBigInt##op(Chili a, Chili b) {                \
        BigInt z, ta, tb;                                       \
        bigIntInit(z);                                          \
        bigInt##op(z, toBigInt(a, ta), toBigInt(b, tb));        \
        return fromBigInt(z);                                   \
    }

#define CHI_BIGINT_UNOP(op)                     \
    Chili _chiSlowBigInt##op(Chili a) {         \
        BigInt z, t;                            \
        bigIntInit(z);                          \
        bigInt##op(z, toBigInt(a, t));          \
        return fromBigInt(z);                   \
    }

#define CHI_BIGINT_TRY_BINOP(op)                                \
    Chili _chiSlowBigIntTry##op(Chili a, Chili b) {             \
        BigInt z, ta, tb;                                       \
        bigIntInit(z);                                          \
        bigInt##op(z, toBigInt(a, ta), toBigInt(b, tb));        \
        return tryFromBigInt(z);                                \
    }

CHI_BIGINT_TRY_BINOP(Sub)
CHI_BIGINT_TRY_BINOP(Add)
CHI_BIGINT_TRY_BINOP(Mul)
CHI_BIGINT_BINOP(Div)
CHI_BIGINT_BINOP(Mod)
CHI_BIGINT_BINOP(Quot)
CHI_BIGINT_BINOP(And)
CHI_BIGINT_BINOP(Or)
CHI_BIGINT_BINOP(Xor)
CHI_BIGINT_BINOP(Rem)
CHI_BIGINT_UNOP(Not)
CHI_BIGINT_UNOP(Neg)

#undef CHI_BIGINT_TRY_BINOP
#undef CHI_BIGINT_BINOP
#undef CHI_BIGINT_UNOP
