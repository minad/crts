#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define _z_inl         static inline __attribute__((always_inline))
#define _z_wu          __attribute__ ((warn_unused_result))
#define _z_swap(a, b)  ({ typeof(a) _z_swap = a; a = b; b = _z_swap; })
#define _z_min(a, b)   ({ typeof(a) _z_min1 = (a), _z_min2 = (b); _z_min1 < _z_min2 ? _z_min1 : _z_min2; })
#define _z_max(a, b)   ({ typeof(a) _z_max1 = (a), _z_max2 = (b); _z_max1 > _z_max2 ? _z_max1 : _z_max2; })
#define _z_unlikely(x) __builtin_expect(!!(x), false)
#define _z_digits(n)    (((n) + Z_BITS - 1) / Z_BITS)
#define z_err           ((z_int){.err=true})
#define z_zero          ((z_int){.size=0})

#if Z_GMP
#  include "zgmp.h"
#else
#  include "znogmp.h"
#endif

typedef struct {
    z_digit* d;  // digits
    z_size size; // number of digits
    z_size alloc;  // allocated digits
    bool neg;    // integer is negative
    bool err;    // true if allocation failed
} z_int;

_z_inl _z_wu int32_t z_cmp(z_int a, z_int b) {
    if (a.neg != b.neg)
        return a.neg ? -1 : 1;
    if (a.neg)
        _z_swap(a, b);
    if (a.size != b.size)
        return a.size < b.size ? -1 : 1;
    int32_t cmp = zd_cmp(a.d, b.d, a.size);
    return cmp ? (cmp > 0 ? 1 : -1) : 0;
}

_z_inl void z_free(z_int a) {
    if (a.alloc)
        Z_FREE(a.d);
}

#define z_frees(...)                                                    \
    ({                                                                  \
        z_int _z_frees[] = { __VA_ARGS__ };                             \
        for (size_t _z_frees_idx = 0; _z_frees_idx < sizeof(_z_frees)/sizeof(z_int); ++_z_frees_idx) \
            z_free(_z_frees[_z_frees_idx]);                             \
    })

_z_inl _z_wu z_int _z_new(bool err, bool neg, z_size size, z_size alloc) {
    if (_z_unlikely(err))
        return z_err;
    z_digit* digit = 0;
    if (alloc) {
        digit = (z_digit*)Z_ALLOC(sizeof (z_digit) * (size_t)alloc);
        if (_z_unlikely(!digit))
            return z_err;
    }
    return (z_int){ .neg = neg, .size = size, .alloc = alloc, .d = digit };
}
#define _z_new_checked(lasterr, neg, size, alloc)                       \
    ({                                                                  \
        z_int _z_new_res = _z_new((lasterr), (neg), (size), (alloc));   \
        if (_z_new_res.err) return z_err; _z_new_res;                   \
    })                                                                  \

_z_inl void _z_grow(z_int* a, z_digit b) {
    a->d[a->size] = b;
    a->size += !!b;
}

_z_inl void _z_trim(z_int* a) {
    Z_ASSERT(!a->err);
    while (a->size && !a->d[a->size - 1])
        --a->size;
    if (!a->size)
        a->neg = false;
}

_z_inl void _z_trim_1(z_int* a) {
    Z_ASSERT(!a->err);
    Z_ASSERT(a->size > 0);
    if (!a->d[a->size - 1])
        --a->size;
}

_z_inl _z_wu z_int _z_addsub_1(z_int a, z_digit b, bool aneg, bool bneg) {
    z_int r = _z_new_checked(a.err, aneg, a.size, a.size + 1);
    if (!a.size) {
        r.d[0] = b;
        r.neg = bneg;
        r.size = 1;
        return r;
    }

    if (aneg == bneg) {
        _z_grow(&r, zd_add_1(r.d, a.d, a.size, b));
    } else if (a.size == 1 && a.d[0] < b) {
        r.d[0] = b - a.d[0];
        r.neg = bneg;
    } else {
        zd_sub_1(r.d, a.d, a.size, b);
        _z_trim(&r);
    }

    return r;
}

_z_inl _z_wu z_int z_add_1(z_int a, z_digit b) {
    return _z_addsub_1(a, b, a.neg, false);
}

_z_inl _z_wu z_int z_sub_1(z_int a, z_digit b) {
    return _z_addsub_1(a, b, a.neg, true);
}

_z_inl _z_wu z_int _z_addsub(z_int a, z_int b, bool aneg, bool bneg) {
    if (a.size < b.size) {
        _z_swap(aneg, bneg);
        _z_swap(a, b);
    }

    if (b.size == 1)
        return _z_addsub_1(a, b.d[0], aneg, bneg);

    z_int r = _z_new_checked(a.err || b.err, aneg, a.size, a.size + 1);
    if (aneg == bneg) {
        _z_grow(&r, zd_add(r.d, a.d, a.size, b.d, b.size));
    } else if (a.size != b.size) {
        zd_sub(r.d, a.d, a.size, b.d, b.size);
        _z_trim(&r);
    } else {
        if (zd_cmp(a.d, b.d, a.size) < 0) {
            _z_swap(a, b);
            r.neg = bneg;
        }
        zd_sub_n(r.d, a.d, b.d, a.size);
        _z_trim(&r);
    }

    return r;
}

_z_inl _z_wu z_int z_sub(z_int a, z_int b) {
    return _z_addsub(a, b, a.neg, b.size ? !b.neg : false);
}

_z_inl _z_wu z_int z_add(z_int a, z_int b) {
    return _z_addsub(a, b, a.neg, b.neg);
}

_z_inl _z_wu bool _z_identical(z_int a, z_int b) {
    return a.d == b.d && a.size == b.size && a.neg == b.neg;
}

_z_inl _z_wu z_int z_mul(z_int a, z_int b) {
    if (a.size < b.size)
        _z_swap(a, b);
    if (!b.size)
        return _z_new(a.err || b.err, false, 0, 0);
    z_int r = _z_new_checked(a.err || b.err, a.neg != b.neg, a.size + b.size, a.size + b.size);
    if (_z_identical(a, b)) {
        zd_sqr(r.d, a.d, a.size);
        _z_trim_1(&r);
    } else if (b.size == 1) {
        r.d[a.size] = zd_mul_1(r.d, a.d, a.size, b.d[0]);
        _z_trim_1(&r);
    } else {
        r.size -= !zd_mul(r.d, a.d, a.size, b.d, b.size);
    }
    return r;
}

_z_inl void _z_out(z_int** p, z_int a) {
    if (*p)
        **p = a;
    else
        z_free(a);
}

_z_inl void z_quorem(z_int* qp, z_int* rp, z_int a, z_int b) {
    Z_ASSERT(b.err || b.size); // non-zero
    z_size qsize = _z_max(a.size - b.size + 1, 0);
    z_int q = _z_new(a.err || b.err, a.neg != b.neg && qsize, qsize, qsize),
        r = _z_new(a.err || b.err, a.neg, b.size, b.size);
    if (!r.err && !q.err && !qsize) {
        if (rp) {
            zd_cpy(r.d, a.d, a.size);
            r.size = a.size;
        }
    } else if (r.err || q.err || !zd_divmod(q.d, r.d, a.d, a.size, b.d, b.size)) {
        z_frees(r, q);
        r = q = z_err;
    } else {
        if (rp)
            _z_trim(&r);
        if (qp) {
            _z_trim_1(&q);
            if (!q.size)
                q.neg = false;
        }
    }
    _z_out(&qp, q);
    _z_out(&rp, r);
}

_z_inl _z_wu z_int z_quo(z_int a, z_int b) {
    z_quorem(&a, 0, a, b);
    return a;
}

_z_inl _z_wu z_int z_rem(z_int a, z_int b) {
    z_quorem(0, &a, a, b);
    return a;
}

_z_inl void z_divmod(z_int* dp, z_int* mp, z_int a, z_int b) {
    z_int q, r;
    z_quorem(&q, &r, a, b);
    if (r.size && a.neg != b.neg) {
        if (dp)
            *dp = z_sub_1(q, 1);
        if (mp)
            *mp = z_add(r, b);
        z_frees(q, r);
    } else {
        _z_out(&dp, q);
        _z_out(&mp, r);
    }
}

_z_inl _z_wu z_int z_div(z_int a, z_int b) {
    z_divmod(&a, 0, a, b);
    return a;
}

_z_inl _z_wu z_int z_mod(z_int a, z_int b) {
    z_divmod(0, &a, a, b);
    return a;
}

_z_inl _z_wu z_int _z_unsigned_xor(z_int a, z_int b) {
    if (a.size < b.size)
        _z_swap(a, b);
    z_int r = _z_new_checked(a.err || b.err, false, a.size, a.size);
    if (b.size)
        zd_xor_n(r.d, a.d, b.d, b.size);
    if (a.size > b.size)
        zd_cpy(r.d + b.size, a.d + b.size, a.size - b.size);
    else
        _z_trim(&r);
    return r;
}

_z_inl _z_wu z_int _z_unsigned_and(z_int a, z_int b) {
    z_size size = _z_min(a.size, b.size);
    z_int r = _z_new_checked(a.err || b.err, false, size, size);
    if (r.size) {
        zd_and_n(r.d, a.d, b.d, r.size);
        _z_trim(&r);
    }
    return r;
}

_z_inl _z_wu z_int _z_unsigned_andnot(z_int a, z_int b) {
    z_int r = _z_new_checked(a.err || b.err, false, a.size, a.size);
    z_size size = _z_min(a.size, b.size);
    if (size)
        zd_andnot_n(r.d, a.d, b.d, size);
    if (size < a.size)
        zd_cpy(r.d + size, a.d + size, a.size - size);
    _z_trim(&r);
    return r;
}

_z_inl _z_wu z_int _z_unsigned_or(z_int a, z_int b) {
    if (a.size < b.size)
        _z_swap(a, b);
    z_int r = _z_new_checked(a.err || b.err, false, a.size, a.size);
    if (b.size)
        zd_or_n(r.d, a.d, b.d, b.size);
    if (a.size > b.size)
        zd_cpy(r.d + b.size, a.d + b.size, a.size - b.size);
    return r;
}

_z_inl _z_wu z_int _z_unsigned_dec(z_int a) {
    Z_ASSERT(a.err || a.size);
    z_int r = _z_new_checked(a.err, false, a.size, a.size + 1);
    zd_sub_1(r.d, a.d, a.size, 1);
    _z_trim(&r);
    return r;
}

_z_inl _z_wu z_int z_xor(z_int a, z_int b) {
    if (!a.neg && !b.neg)
        return _z_unsigned_xor(a, b);

    if (a.neg)
        _z_swap(a, b);

    z_int x, y, r;
    x = _z_unsigned_dec(b);
    if (a.neg && b.neg) {
        y = _z_unsigned_dec(a);
        r = _z_unsigned_xor(x, y);
    } else {
        y = _z_unsigned_xor(a, x);
        r = z_add_1(y, 1);
        r.neg = true;
    }
    z_frees(x, y);
    return r;
}

_z_inl _z_wu z_int z_and(z_int a, z_int b) {
    if (!a.neg && !b.neg)
        return _z_unsigned_and(a, b);

    if (a.neg)
        _z_swap(a, b);

    z_int x, y, z, r;
    x = _z_unsigned_dec(b);
    if (a.neg && b.neg) {
        y = _z_unsigned_dec(a);
        z = _z_unsigned_or(x, y);
        r = z_add_1(z, 1);
        r.neg = true;
    } else {
        r = _z_unsigned_andnot(a, x);
        y = z = z_zero;
    }
    z_frees(x, y, z);
    return r;
}

_z_inl _z_wu z_int z_or(z_int a, z_int b) {
    if (!a.neg && !b.neg)
        return _z_unsigned_or(a, b);

    if (a.neg)
        _z_swap(a, b);

    z_int x, y, z, r;
    x = _z_unsigned_dec(b);
    if (a.neg && b.neg) {
        y = _z_unsigned_dec(a);
        z = _z_unsigned_and(x, y);
        r = z_add_1(z, 1);
    } else {
        y = _z_unsigned_andnot(x, a);
        r = z_add_1(y, 1);
        z = z_zero;
    }
    z_frees(x, y, z);
    r.neg = true;
    return r;
}

_z_inl _z_wu z_int z_abs(z_int a) {
    a.neg = false;
    a.alloc = 0;
    return a;
}

_z_inl _z_wu z_int z_neg(z_int a) {
    a.neg = !a.neg && a.size;
    a.alloc = 0;
    return a;
}

_z_inl _z_wu z_int z_not(z_int a) {
    return z_sub_1(z_neg(a), 1);
}

_z_inl _z_wu z_int z_shl(z_int a, uint16_t b) {
    if (!a.size)
        return _z_new(a.err, false, 0, 0);
    z_size n = b / Z_BITS;
    z_int r = _z_new_checked(a.err, a.neg, a.size + n, a.size + n + 1);
    b %= Z_BITS;
    if (b)
        _z_grow(&r, zd_shl(r.d + n, a.d, a.size, b));
    else
        zd_cpy(r.d + n, a.d, a.size);
    zd_zero(r.d, n);
    return r;
}

_z_inl _z_wu uint64_t z_to_u64(z_int a) {
    Z_ASSERT(!a.err);
    z_size i = _z_min(a.size, _z_digits(64));
    uint64_t b = 0;
    while (i --> 0) {
        b <<= 64 <= Z_BITS ? 0 : Z_BITS;
        b |= (uint64_t)a.d[i];
        if (64 <= Z_BITS)
            break;
    }
    return a.neg ? (uint64_t)(-(int64_t)b) : b;
}

_z_inl _z_wu z_int z_from_u64(uint64_t b) {
    z_int r = _z_new_checked(false, false, 0, _z_digits(64));
    while (b) {
        r.d[r.size++] = (z_digit)(b & (((uint64_t)1 << (Z_BITS >= 64 ? 0 : Z_BITS)) - 1));
        if (64 <= Z_BITS)
            break;
        b >>= 64 <= Z_BITS ? 0 : Z_BITS;
    }
    return r;
}

_z_inl _z_wu z_int z_from_i64(int64_t b) {
    z_int r = z_from_u64((uint64_t)(b < 0 ? -b : b));
    r.neg = b < 0;
    return r;
}

_z_inl _z_wu z_int z_shr(z_int a, uint16_t b) {
    z_size n = b / Z_BITS;
    if (a.size <= n && a.neg)
        return z_from_i64(-1);
    if (!a.size || a.size <= n)
        return _z_new(a.err, false, 0, 0);
    z_int r = _z_new_checked(a.err, a.neg, a.size - n, a.size - n);
    b %= Z_BITS;
    if (b) {
        zd_shr(r.d, a.d + n, r.size, b);
        _z_trim_1(&r);
    } else {
        zd_cpy(r.d, a.d + n, r.size);
    }
    return r;
}

_z_inl _z_wu double z_to_d(z_int a) {
    Z_ASSERT(!a.err);
    if (!a.size)
        return 0;
    double d = 0, p = 2.0 * (double)((z_digit)1 << (Z_BITS - 1));
    for (z_size i = a.size; i --> 0;)
        d = d * p + (double)a.d[i];
    return a.neg ? -d : d;
}

_z_inl _z_wu z_int z_from_d(double b) {
    union {
        double   dbl;
        uint64_t bits;
    } cast = { .dbl = b };
    int32_t exp = (int32_t)((cast.bits >> 52) & 0x7FF);
    if (exp == 0x7FF)
        return z_zero; // convention: return 0 for +-inf, NaN
    exp -= 1023 + 52;
    uint64_t frac = (cast.bits & (((uint64_t)1 << 52) - (uint64_t)1)) | ((uint64_t)1 << 52);
    z_int f = z_from_u64(frac),
        r = exp < 0 ? z_shr(f, (uint16_t)-exp) : z_shl(f, (uint16_t)exp);
    z_free(f);
    r.neg = (cast.bits >> 63) && r.size;
    return r;
}

_z_inl _z_wu z_int z_from_b(const uint8_t *buf, size_t size) {
    Z_ASSERT(size);
    z_int r = _z_new_checked(false, false, 0, (z_size)_z_digits(8 * size));
    z_digit c = 0;
    for (size_t s = 0, i = size; i --> 0; ) {
        if (!s) {
            Z_ASSERT((size_t)r.size < _z_digits(8 * size));
            ++r.size;
            r.d[r.size - 1] = c;
        }
        r.d[r.size - 1] |= (z_digit)buf[i] << s;
        s += 8;
        if (s >= Z_BITS) {
            c = Z_BITS % 8 == 0 ? 0 : (z_digit)buf[i] >> (8 + Z_BITS - s);
            s = 0;
        }
    }
    Z_ASSERT(r.d[r.size - 1]);
    return r;
}
