typedef int32_t z_size;

#if Z_BITS == 64
typedef uint64_t z_digit;
typedef __uint128_t _z_ddigit;
#elif Z_BITS == 32
typedef uint32_t z_digit;
typedef uint64_t _z_ddigit;
#elif Z_BITS == 16
typedef uint16_t z_digit;
typedef uint32_t _z_ddigit;
#elif Z_BITS == 8
typedef uint8_t z_digit;
typedef uint16_t _z_ddigit;
#else
#error Invalid Z_BITS value
#endif

_z_inl z_digit zd_shl(z_digit* r, const z_digit* a, z_size n, unsigned int s) {
    Z_ASSERT(s > 0);
    Z_ASSERT(s < Z_BITS);
    z_digit c = 0;
    while (n --> 0) {
        z_digit x = (*a << s) | c;
        c = *a++ >> (Z_BITS - s);
        *r++ = x;
    }
    return c;
}

_z_inl z_digit zd_shr(z_digit* r, const z_digit* a, z_size n, unsigned int s) {
    Z_ASSERT(s > 0);
    Z_ASSERT(s < Z_BITS);
    z_digit c = 0;
    r += n;
    a += n;
    while (n --> 0) {
        z_digit x = (*--a >> s) | c;
        c = *a << (Z_BITS - s);
        *--r = x;
    }
    return c;
}

_z_inl int32_t zd_cmp(const z_digit* a, const z_digit* b, z_size n) {
    while (n --> 0) {
        if (a[n] != b[n])
            return a[n] < b[n] ? -1 : 1;
    }
    return 0;
}

// TODO provide implementation which does not use _z_ddigit?
_z_inl void _zd_dd_mul(z_digit a, z_digit b, z_digit* hp, z_digit* lp) {
    _z_ddigit w = (_z_ddigit)a * (_z_ddigit)b;
    *lp = (z_digit)w;
    *hp = (z_digit)(w >> Z_BITS);
}

// TODO provide implementation which does not use _z_ddigit?
// uint128 div mod synthesises a call to __udivmodti3
_z_inl void _zd_dd_div(z_digit ah, z_digit al, z_digit b, z_digit* qp, z_digit* rp) {
    Z_ASSERT(ah <= b);
    _z_ddigit w = ((_z_ddigit)ah << Z_BITS) | (_z_ddigit)al;
    *qp = (z_digit)(w / b);
    *rp = (z_digit)(w % b);
}

_z_inl z_digit zd_mul_1(z_digit* r, const z_digit* a, z_size n, z_digit b) {
    z_digit c = 0, h, l;
    while (n --> 0) {
        _zd_dd_mul(*a++, b, &h, &l);
        c = h + (z_digit)__builtin_add_overflow(c, l, r++);
    }
    return c;
}

_z_inl z_digit zd_submul_1(z_digit* r, const z_digit* a, z_size n, z_digit b) {
    z_digit c = 0, h, l;
    while (n --> 0) {
        _zd_dd_mul(*a++, b, &h, &l);
        z_digit c1 = __builtin_sub_overflow(*r, l, &l);
        z_digit c2 = __builtin_sub_overflow(l, c, r);
        c = h + c1 + c2;
        ++r;
    }
    return c;
}

_z_inl z_digit zd_addmul_1(z_digit* r, const z_digit* a, z_size n, z_digit b) {
    z_digit c = 0, h, l;
    while (n --> 0) {
        _zd_dd_mul(*a++, b, &h, &l);
        z_digit c1 = __builtin_add_overflow(*r, l, &l);
        z_digit c2 = __builtin_add_overflow(l, c, r);
        c = h + c1 + c2;
        ++r;
    }
    return c;
}

// TODO: Implement Karatsuba for a certain cutoff
_z_inl z_digit zd_mul(z_digit* r, const z_digit* a, z_size n, const z_digit* b, z_size m) {
    Z_ASSERT(n >= m);
    r[n] = zd_mul_1(r, a, n, *b++);
    while (m --> 1) {
        ++r;
        r[n] = zd_addmul_1(r, a, n, *b++);
    }
    return r[n];
}

_z_inl z_digit zd_sqr(z_digit* r, const z_digit* a, z_size n) {
    return zd_mul(r, a, n, a, n);
}

_z_inl z_digit zd_add_1(z_digit* r, const z_digit* a, z_size n, z_digit b) {
    z_digit c = b;
    while (n --> 0)
        c = __builtin_add_overflow(*a++, c, r++);
    return c;
}

_z_inl z_digit zd_sub_1(z_digit* r, const z_digit* a, z_size n, z_digit b) {
    z_digit c = b;
    while (n --> 0)
        c = __builtin_sub_overflow(*a++, c, r++);
    return c;
}

_z_inl z_digit zd_add_n(z_digit* r, const z_digit* a, const z_digit* b, z_size n) {
    z_digit c = 0;
    while (n --> 0) {
        z_digit c1 = __builtin_add_overflow(*a++, *b++, r);
        z_digit c2 = __builtin_add_overflow(*r, c, r);
        c = c1 + c2;
        ++r;
    }
    return c;
}

_z_inl z_digit zd_add(z_digit* r, const z_digit* a, z_size n, const z_digit* b, z_size m) {
    Z_ASSERT(n >= m);
    z_digit c = zd_add_n(r, a, b, m);
    return n == m ? c : zd_add_1(r + m, a + m, n - m, c);
}

_z_inl z_digit zd_sub_n(z_digit* r, const z_digit* a, const z_digit* b, z_size n) {
    z_digit c = 0;
    while (n --> 0) {
        z_digit c1 = __builtin_sub_overflow(*a++, *b++, r);
        z_digit c2 = __builtin_sub_overflow(*r, c, r);
        c = c1 + c2;
        ++r;
    }
    return c;
}

_z_inl z_digit zd_sub(z_digit* r, const z_digit* a, z_size n, const z_digit* b, z_size m) {
    Z_ASSERT(n >= m);
    z_digit c = zd_sub_n(r, a, b, m);
    return n == m ? c : zd_sub_1(r + m, a + m, n - m, c);
}

_z_inl void zd_or_n(z_digit* r, const z_digit* a, const z_digit* b, z_size n) {
    while (n --> 0)
        *r++ = *a++ | *b++;
}

_z_inl void zd_and_n(z_digit* r, const z_digit* a, const z_digit* b, z_size n) {
    while (n --> 0)
        *r++ = *a++ & *b++;
}

_z_inl void zd_andnot_n(z_digit* r, const z_digit* a, const z_digit* b, z_size n) {
    while (n --> 0)
        *r++ = *a++ & ~*b++;
}

_z_inl void zd_xor_n(z_digit* r, const z_digit* a, const z_digit* b, z_size n) {
    while (n --> 0)
        *r++ = *a++ ^ *b++;
}

_z_inl z_digit _zd_div_1(z_digit* q, const z_digit* u, z_size m, z_digit v) {
    z_digit c = 0;
    while (m --> 0)
        _zd_dd_div(c, u[m], v, q + m, &c);
    return c;
}

_z_inl void _zd_div_knuth_step(z_digit* q, z_digit* u, z_digit* v, z_size n, z_size j) {
    // Estimate Q of q[j]
    z_digit Q, R, h, l;
    bool Qh = u[j + n] == v[n - 1];
    _zd_dd_div(u[j + n], u[j + n - 1], v[n - 1], &Q, &R);

    // Calculate Q
    do {
        if (!Qh)   {
            _zd_dd_mul(Q, v[n - 2], &h, &l);
            if (h < R || (h == R && l <= u[j + n - 2]))
                break;
        }
        Qh = !__builtin_sub_overflow(Q, 1, &Q) && Qh;
    } while (!__builtin_add_overflow(R, v[n - 1], &R));

    // Multiply and subtract
    if (__builtin_sub_overflow(u[j + n], zd_submul_1(u + j, v, n, Q), &u[j + n])) {
        // Add back if we subtracted too much
        --Q;
        u[j + n] += zd_add_n(u + j, u + j, v, n);
    }

    q[j] = Q;
}

// Knuth's algorithm D, adapted from Hacker's Delight by Warren
_z_inl bool _zd_div_knuth(z_digit* q, z_digit* r,
                          const z_digit* u, z_size m,
                          const z_digit* v, z_size n) {
    Z_ASSERT(m >= n);
    Z_ASSERT(n > 0);
    Z_ASSERT(v[n - 1]);

    // Allocate scratch space
    unsigned s = (unsigned)(__builtin_clzll(v[n - 1]) - 8 * (int)sizeof(long long) + Z_BITS);
    z_size needed = n + m + 1;
    z_digit scratch[Z_SCRATCH],
        *vn = _z_unlikely(needed > Z_SCRATCH)
        ? (z_digit*)Z_ALLOC(sizeof (z_digit) * (size_t)needed)
        : scratch,
        *un = vn + n;
    if (_z_unlikely(!vn))
        return false;

    // Normalize
    if (s) {
        zd_shl(vn, v, n, s);
        un[m] = zd_shl(un, u, m, s);
    } else {
        _z_cpy(vn, v, n);
        _z_cpy(un, u, m);
        un[m] = 0;
    }

    for (z_size j = m - n; j >= 0; --j)
        _zd_div_knuth_step(q, un, vn, n, j);

    // Denormalize remainder
    if (s)
        zd_shr(r, un, n, s);
    else
        _z_cpy(r, un, n);

    if (vn != scratch)
        Z_FREE(vn);

    return true;
}

_z_inl bool zd_divmod(z_digit* q, z_digit* r,
                      const z_digit* u, z_size m,
                      const z_digit* v, z_size n) {
    Z_ASSERT(m >= n);
    Z_ASSERT(n > 0);
    Z_ASSERT(v[n - 1]);
    if (n == 1) {
        r[0] = _zd_div_1(q, u, m, v[0]);
        return true;
    }
    return _zd_div_knuth(q, r, u, m, v, n);
}
