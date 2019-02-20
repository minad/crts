static void* bigIntCalloc(size_t nelem, size_t size) {
    size *= nelem;
    return memset(bigIntAlloc(size), 0, size);
}

#define MP_USE_ENUMS
#define MP_USE_MEMSET
#define MP_NO_ZERO_ON_FREE
#define MP_NO_FILE
#define MP_FIXED_CUTOFFS
#define MP_FREE bigIntFree
#define MP_MALLOC bigIntAlloc
#define MP_REALLOC bigIntRealloc
#define MP_CALLOC bigIntCalloc
#if CHI_ARCH_BITS == 32
#define MP_PREC 4
#else
#define MP_PREC 2
#endif
CHI_WARN_OFF(frame-larger-than=)
#include "../tommath/tommath_amalgam.h"
CHI_WARN_ON

typedef struct { mp_int _v[1]; } BigIntStruct;
typedef BigIntStruct BigInt[1];

#define BIGINT_CHECK(op)                        \
    ({                                          \
        mp_err _res = (op);                     \
        CHI_ASSERT(_res == MP_OKAY);            \
        _res;                                   \
    })

CHI_INL int32_t bigIntCmp(const BigInt a, const BigInt b) {
    int cmp = mp_cmp(a->_v, b->_v);
    return cmp ? (cmp > 0 ? 1 : -1) : 0;
}

CHI_INL void bigIntClear(BigInt a) {
    mp_clear(a->_v);
}

CHI_INL void bigIntInit(BigInt a) {
    int res = mp_init(a->_v);
    if (res != MP_OKAY) {
        CHI_CLEAR_ARRAY(a->_v);
        BIGINT_CHECK(res);
    }
}

CHI_INL void bigIntInitI(BigInt a, int64_t i) {
    BIGINT_CHECK(mp_init(a->_v));
    mp_set_i64(a->_v, i);
}

CHI_INL void bigIntInitU(BigInt a, uint64_t i) {
    BIGINT_CHECK(mp_init(a->_v));
    mp_set_u64(a->_v, i);
}

CHI_INL void bigIntInitF(BigInt a, double x) {
    BIGINT_CHECK(mp_init(a->_v));
    CHI_IGNORE_RESULT(mp_set_double(a->_v, x)); // do not check for error due to NaN and inf
}

CHI_INL void bigIntInitB(BigInt a, const uint8_t* s, size_t len) {
    BIGINT_CHECK(mp_init(a->_v));
    BIGINT_CHECK(mp_read_unsigned_bin(a->_v, s, (int)len));
}

CHI_INL void bigIntAdd(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_add(a->_v, b->_v, r->_v));
}

CHI_INL void bigIntMul(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_mul(a->_v, b->_v, r->_v));
}

CHI_INL void bigIntDiv(BigInt r, const BigInt a, const BigInt b) {
    mp_int mod;
    BIGINT_CHECK(mp_init(&mod));
    BIGINT_CHECK(mp_div(a->_v, b->_v, r->_v, &mod));
    if (!mp_iszero(&mod) && mod.sign != b->_v->sign)
        BIGINT_CHECK(mp_sub_d(r->_v, 1, r->_v));
    mp_clear(&mod);
}

CHI_INL void bigIntMod(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_mod(a->_v, b->_v, r->_v));
}

CHI_INL void bigIntQuot(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_div(a->_v, b->_v, r->_v, 0));
}

CHI_INL void bigIntRem(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_div(a->_v, b->_v, 0, r->_v));
}

CHI_INL void bigIntNot(BigInt r, const BigInt a) {
    BIGINT_CHECK(mp_complement(a->_v, r->_v));
}

CHI_INL void bigIntNeg(BigInt r, const BigInt a) {
    BIGINT_CHECK(mp_neg(a->_v, r->_v));
}

CHI_INL void bigIntAnd(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_and(a->_v, b->_v, r->_v));
}

CHI_INL void bigIntOr(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_or(a->_v, b->_v, r->_v));
}

CHI_INL void bigIntXor(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_xor(a->_v, b->_v, r->_v));
}

CHI_INL void bigIntSub(BigInt r, const BigInt a, const BigInt b) {
    BIGINT_CHECK(mp_sub(a->_v, b->_v, r->_v));
}

CHI_INL void bigIntShl(BigInt r, const BigInt a, uint16_t b) {
    BIGINT_CHECK(mp_mul_2d(a->_v, b, r->_v));
}

CHI_INL void bigIntShr(BigInt r, const BigInt a, uint16_t b) {
    BIGINT_CHECK(mp_signed_rsh(a->_v, b, r->_v));
}

CHI_INL double bigIntToF(const BigInt a) {
    return mp_get_double(a->_v);
}

CHI_INL uint64_t bigIntToU(const BigInt a) {
    return mp_get_u64(a->_v);
}

CHI_INL size_t bigIntStrSize(const BigInt a) {
    int size;
    BIGINT_CHECK(mp_radix_size(a->_v, 10, &size));
    return (size_t)size;
}

CHI_INL const char* bigIntStr(const BigInt a, char* s) {
    BIGINT_CHECK(mp_toradix(a->_v, s, 10));
    return s;
}

CHI_INL void bigIntUnpack(const BigInt a, bool* sign, size_t* size, size_t* alloc, void** data) {
    *sign = mp_isneg(a->_v);
    *size = sizeof (mp_digit) * (size_t)a->_v->used;
    *alloc = sizeof (mp_digit) * (size_t)a->_v->alloc;
    *data = a->_v->dp;
}

CHI_INL void bigIntPack(BigInt a, bool sign, size_t size, void* data) {
    a->_v->sign = sign ? MP_NEG : MP_ZPOS;
    a->_v->used = a->_v->alloc = (int)(size / sizeof (mp_digit));
    a->_v->dp = (mp_digit*)data;
}

void chiBigIntSetup(void) {}
