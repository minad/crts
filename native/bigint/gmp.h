#include <gmp.h>

typedef struct { mpz_t _v; } BigIntStruct;
typedef BigIntStruct BigInt[1];

// There seems to be a bug in the mpz_get_d upstream version?
// TODO replace with faster version, which directly manipulates the double bits.
CHI_INL double _mpz_get_d(const mpz_t z) {
    size_t s = mpz_size(z);
    if (!s)
        return 0;
    const mp_limb_t* limb = mpz_limbs_read(z);
    double d = 0, p = pow(2, GMP_LIMB_BITS);
    for (size_t i = s; i --> 0;)
        d = d * p + (double)limb[i];
    return mpz_sgn(z) * d;
}

// TODO try to get this into upstream gmp
CHI_INL void _mpz_init_ull(mpz_t z, unsigned long long i) {
    _Static_assert(sizeof (long) == sizeof (long long) || sizeof (int) == 4, "Unsupported sizes");
    if (sizeof (long) == sizeof (long long)) {
        mpz_init_set_ui(z, (unsigned long)i);
    } else {
        mpz_init_set_ui(z, (unsigned long)(i >> 32));
        mpz_mul_2exp(z, z, 32);
        mpz_add_ui(z, z, (unsigned long)i);
    }
}

// TODO try to get this into upstream gmp
CHI_INL void _mpz_init_sll(mpz_t z, long long i) {
    _mpz_init_ull(z, (unsigned long long)(i < 0 ? -i : i));
    if (i < 0)
        z->_mp_size = -z->_mp_size;
}

// Works also for negative integers which are in two complement's representation
// TODO try to get this into upstream gmp
CHI_INL unsigned long long _mpz_get_ull(const mpz_t z) {
    _Static_assert(sizeof (long) == sizeof (long long) || sizeof (int) == 4, "Unsupported sizes");
    unsigned long long ret;
    if (sizeof (long) == sizeof (long long)) {
        ret = mpz_get_ui(z);
    } else {
        mpz_t t;
        mpz_init(t);
        mpz_fdiv_q_2exp(t, z, 32);
        ret = (((unsigned long long)mpz_get_ui(t)) << 32) | mpz_get_ui(t);
        mpz_clear(t);
    }
    return (unsigned long long)((long long)ret * (long long)mpz_sgn(z));
}

CHI_INL int32_t bigIntCmp(const BigInt a, const BigInt b) {
    int cmp = mpz_cmp(a->_v, b->_v);
    return cmp ? (cmp > 0 ? 1 : -1) : 0;
}

CHI_INL void bigIntClear(BigInt a) {
    mpz_clear(a->_v);
}

CHI_INL void bigIntInit(BigInt a) {
    mpz_init(a->_v);
}

CHI_INL void bigIntInitI(BigInt a, int64_t i) {
    _mpz_init_sll(a->_v, i);
}

CHI_INL void bigIntInitU(BigInt a, uint64_t i) {
    _mpz_init_ull(a->_v, i);
}

CHI_INL void bigIntInitF(BigInt a, double f) {
    mpz_init_set_d(a->_v, __builtin_isfinite(f) ? f : 0);
}

CHI_INL void bigIntInitB(BigInt a, const uint8_t* s, size_t len) {
    mpz_init(a->_v);
    mpz_import(a->_v, len, 1, 1, 0, 0, (const char*)s);
}

CHI_INL void bigIntAdd(BigInt r, const BigInt a, const BigInt b) {
    mpz_add(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntMul(BigInt r, const BigInt a, const BigInt b) {
    mpz_mul(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntDiv(BigInt r, const BigInt a, const BigInt b) {
    mpz_fdiv_q(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntMod(BigInt r, const BigInt a, const BigInt b) {
    mpz_fdiv_r(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntQuot(BigInt r, const BigInt a, const BigInt b) {
    mpz_tdiv_q(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntRem(BigInt r, const BigInt a, const BigInt b) {
    mpz_tdiv_r(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntNot(BigInt r, const BigInt a) {
    mpz_com(r->_v, a->_v);
}

CHI_INL void bigIntNeg(BigInt r, const BigInt a) {
    mpz_neg(r->_v, a->_v);
}

CHI_INL void bigIntAnd(BigInt r, const BigInt a, const BigInt b) {
    mpz_and(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntOr(BigInt r, const BigInt a, const BigInt b) {
    mpz_ior(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntXor(BigInt r, const BigInt a, const BigInt b) {
    mpz_xor(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntSub(BigInt r, const BigInt a, const BigInt b) {
    mpz_sub(r->_v, a->_v, b->_v);
}

CHI_INL void bigIntShl(BigInt r, const BigInt a, uint16_t b) {
    mpz_mul_2exp(r->_v, a->_v, b);
}

CHI_INL void bigIntShr(BigInt r, const BigInt a, uint16_t b) {
    mpz_fdiv_q_2exp(r->_v, a->_v, b);
}

CHI_INL double bigIntToF(const BigInt a) {
    return _mpz_get_d(a->_v);
}

CHI_INL uint64_t bigIntToU(const BigInt a) {
    return _mpz_get_ull(a->_v);
}

CHI_INL size_t bigIntStrSize(const BigInt a) {
    return mpz_sizeinbase(a->_v, 10) + 2;
}

CHI_INL const char* bigIntStr(const BigInt a, char* s) {
    return mpz_get_str(s, 10, a->_v);
}

CHI_INL void bigIntUnpack(const BigInt a, bool* sign, size_t* size, size_t* alloc, void** data) {
    *sign = a->_v->_mp_size < 0;
    *size = sizeof (mp_limb_t) * (size_t)(a->_v->_mp_size < 0 ? -a->_v->_mp_size : a->_v->_mp_size);
    *alloc = sizeof (mp_limb_t) * (size_t)a->_v->_mp_alloc;
    *data = a->_v->_mp_d;
}

CHI_INL void bigIntPack(BigInt a, bool sign, size_t size, void* data) {
    a->_v->_mp_alloc = (int)(size / sizeof (mp_limb_t));
    a->_v->_mp_size = sign ? -a->_v->_mp_alloc : a->_v->_mp_alloc;
    a->_v->_mp_d = (mp_limb_t*)data;
}

void chiBigIntSetup(void) {
    // We use our own allocation functions for GMP
    // which use the garbage collected heap.
    mp_set_memory_functions(bigIntAlloc, bigIntRealloc, bigIntFree);
}
