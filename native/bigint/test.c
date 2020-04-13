#include <assert.h>
#include <gmp.h>
#include <stdlib.h>
#include <time.h>

#define Z_ALLOC(x)  malloc(x)
#define Z_FREE(x)   free(x)
#define Z_ASSERT(x) assert(x)
#define Z_SCRATCH   64
#include "z.h"

#define TEST_SMALL 200
#define TEST_LARGE 5000
#define TEST_RUNS  5000

typedef void (*mpz_bin_t)(mpz_ptr, mpz_srcptr, mpz_srcptr);
typedef z_int (*z_bin_t)(z_int, z_int);

static gmp_randstate_t randstate;

static z_int mpz_to_z(mpz_srcptr in) {
    z_int out = { .neg = mpz_sgn(in) < 0, .size = _z_digits((z_size)(mpz_size(in) * GMP_NUMB_BITS)) };
    out.alloc = out.size;
    out.d = calloc((size_t)out.size, sizeof (z_digit));
    mpz_export(out.d, 0, -1, Z_BITS/8, 0, 0, in);
    _z_trim(&out);
    return out;
}

static void z_to_mpz(mpz_ptr out, z_int in) {
    mpz_init(out);
    mpz_import(out, (size_t)in.size, -1, Z_BITS/8, 0, 0, in.d);
    if (in.neg)
        mpz_neg(out, out);
}

static void check_bin(const char* name, bool nonzero, mpz_bin_t f1, z_bin_t f2, mpz_ptr a1, mpz_ptr b1) {
    if (nonzero && !b1->_mp_size)
        return;

    mpz_t r1;
    mpz_init(r1);
    f1(r1, a1, b1);

    z_int a2 = mpz_to_z(a1),
        b2 = mpz_to_z(b1),
        r2 = mpz_to_z(r1),
        u2 = f2(a2, b2);

    if (u2.err) {
        gmp_printf("%s errored\n", name);
    } else if (z_cmp(u2, r2) != 0) {
        mpz_t u1;
        z_to_mpz(u1, u2);
        gmp_printf("%s returned invalid result\na = %Zd\nb = %Zd\nr = %Zd\nR = %Zd\n",
                   name, a1, b1, r1, u1);
        mpz_clear(u1);
    }

    mpz_clear(r1);

    z_frees(a2, b2, r2, u2);
}

static void check_bin_long(const char* name, bool nonzero, mpz_bin_t f1, z_bin_t f2, long x, long y) {
    mpz_t a1, b1;
    mpz_init(a1);
    mpz_init(b1);
    mpz_set_si(a1, x);
    mpz_set_si(b1, y);
    check_bin(name, nonzero, f1, f2, a1, b1);
    check_bin(name, nonzero, f1, f2, a1, a1);
    check_bin(name, nonzero, f1, f2, b1, b1);
    mpz_clear(a1);
    mpz_clear(b1);
}

static void check_bin_rand_neg(const char* name, bool nonzero, mpz_bin_t f1, z_bin_t f2, mpz_ptr a1, mpz_ptr b1) {
    // both positive
    check_bin(name, nonzero, f1, f2, a1, b1);

    // one negative
    mpz_neg(a1, a1);
    check_bin(name, nonzero, f1, f2, a1, b1);

    // one negative
    mpz_neg(b1, b1);
    check_bin(name, nonzero, f1, f2, a1, b1);

    // both negative
    mpz_neg(a1, a1);
    mpz_neg(b1, b1);
    check_bin(name, nonzero, f1, f2, a1, b1);

    // same values
    check_bin(name, nonzero, f1, f2, a1, a1);
    check_bin(name, nonzero, f1, f2, b1, b1);

    // same values both negative
    mpz_neg(b1, b1);
    mpz_neg(a1, a1);
    check_bin(name, nonzero, f1, f2, a1, a1);
    check_bin(name, nonzero, f1, f2, b1, b1);
}

static void check_bin_rand(const char* name, bool nonzero, mpz_bin_t f1, z_bin_t f2) {
    {  // two large values
        mpz_t a1, b1;
        mpz_init(a1);
        mpz_init(b1);
        mpz_urandomb(a1, randstate, TEST_LARGE);
        mpz_urandomb(b1, randstate, TEST_LARGE);
        check_bin_rand_neg(name, nonzero, f1, f2, a1, b1);
        mpz_clear(a1);
        mpz_clear(b1);
    }
    {  // two small values
        mpz_t a1, b1;
        mpz_init(a1);
        mpz_init(b1);
        mpz_urandomb(a1, randstate, TEST_SMALL);
        mpz_urandomb(b1, randstate, TEST_SMALL);
        check_bin_rand_neg(name, nonzero, f1, f2, a1, b1);
        mpz_clear(a1);
        mpz_clear(b1);
    }
    {  // large and small value
        mpz_t a1, b1;
        mpz_init(a1);
        mpz_init(b1);
        mpz_urandomb(a1, randstate, TEST_LARGE);
        mpz_urandomb(b1, randstate, TEST_SMALL);
        check_bin_rand_neg(name, nonzero, f1, f2, a1, b1);
        mpz_clear(a1);
        mpz_clear(b1);
    }
    {  // small and large value
        mpz_t a1, b1;
        mpz_init(a1);
        mpz_init(b1);
        mpz_urandomb(a1, randstate, TEST_SMALL);
        mpz_urandomb(b1, randstate, TEST_LARGE);
        check_bin_rand_neg(name, nonzero, f1, f2, a1, b1);
        mpz_clear(a1);
        mpz_clear(b1);
    }
    {  // values of the same size
        mpz_t a1, b1;
        mpz_init(a1);
        mpz_init(b1);
        mpz_urandomb(a1, randstate, TEST_LARGE);
        mpz_urandomb(b1, randstate, TEST_LARGE);

        a1->_mp_size = b1->_mp_size = (int)_z_min(mpz_size(a1), mpz_size(b1));
        a1->_mp_d[_z_max(a1->_mp_size - 1, 0)] = 1;
        b1->_mp_d[_z_max(a1->_mp_size - 1, 0)] = 1;

        check_bin_rand_neg(name, nonzero, f1, f2, a1, b1);
        mpz_clear(a1);
        mpz_clear(b1);
    }
}

static void test_bin(const char* name, bool nonzero, mpz_bin_t f1, z_bin_t f2) {
    for (long i = -100; i < 100; ++i)
        for (long j = -100; j < 100; ++j)
            check_bin_long(name, nonzero, f1, f2, i, j);

    for (size_t i = 0; i < TEST_RUNS; ++i)
        check_bin_rand(name, nonzero, f1, f2);
}

int main(void) {
    gmp_randinit_default(randstate);
    gmp_randseed_ui(randstate, (unsigned long)time(0));
    test_bin("and", false, mpz_and, z_and);
    test_bin("or", false, mpz_ior, z_or);
    test_bin("xor", false, mpz_xor, z_xor);
    test_bin("add", false, mpz_add, z_add);
    test_bin("sub", false, mpz_sub, z_sub);
    test_bin("mul", false, mpz_mul, z_mul);
    test_bin("quo", true, mpz_tdiv_q, z_quo);
    test_bin("rem", true, mpz_tdiv_r, z_rem);
    test_bin("div", true, mpz_fdiv_q, z_div);
    test_bin("mod", true, mpz_fdiv_r, z_mod);
    gmp_randclear(randstate);
    // TODO test z_from_b
    // TODO test z_from_d
    // TODO test z_from_i64
    // TODO test z_from_u64
    // TODO test z_not
    // TODO test z_shl
    // TODO test z_shr
    // TODO test z_to_d
    // TODO test z_to_u64
    return 0;
}
