/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
#define MP_ADD_C
#define MP_ADD_D_C
#define MP_AND_C
#define MP_CLAMP_C
#define MP_CLEAR_C
#define MP_CMP_C
#define MP_CMP_MAG_C
#define MP_COMPLEMENT_C
#define MP_COPY_C
#define MP_COUNT_BITS_C
#define MP_DIV_C
#define MP_DIV_2D_C
#define MP_DIV_D_C
#define MP_EXCH_C
#define MP_GET_DOUBLE_C
#define MP_GET_I64_C
#define MP_GET_MAG64_C
#define MP_GROW_C
#define MP_INIT_C
#define MP_INIT_COPY_C
#define MP_INIT_SIZE_C
#define MP_LSHD_C
#define MP_MOD_C
#define MP_MOD_2D_C
#define MP_MUL_C
#define MP_MUL_2D_C
#define MP_MUL_D_C
#define MP_NEG_C
#define MP_OR_C
#define MP_RADIX_SIZE_C
#define MP_RADIX_SMAP_C
#define MP_READ_UNSIGNED_BIN_C
#define MP_RSHD_C
#define MP_SET_DOUBLE_C
#define MP_SET_I64_C
#define MP_SET_U64_C
#define MP_SIGNED_RSH_C
#define MP_SUB_C
#define MP_SUB_D_C
#define MP_TORADIX_C
#define MP_XOR_C
#define MP_ZERO_C
#define S_MP_ADD_C
#define S_MP_KARATSUBA_MUL_C
#define S_MP_MUL_DIGS_C
#define S_MP_MUL_DIGS_FAST_C
#define S_MP_REVERSE_C
#define S_MP_SUB_C
#ifndef TOMMATH_H_
#define TOMMATH_H_

#include <stdint.h>

#ifdef LTM_NO_FILE
#  warning LTM_NO_FILE has been deprecated, use MP_NO_FILE.
#  define MP_NO_FILE
#endif

#ifndef MP_NO_FILE
#  include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* MS Visual C++ doesn't have a 128bit type for words, so fall back to 32bit MPI's (where words are 64bit) */
#if (defined(_MSC_VER) || defined(__LLP64__) || defined(__e2k__) || defined(__LCC__)) && !defined(MP_64BIT)
#   define MP_32BIT
#endif

/* detect 64-bit mode if possible */
#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64) || \
    defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || \
    defined(__s390x__) || defined(__arch64__) || defined(__aarch64__) || \
    defined(__sparcv9) || defined(__sparc_v9__) || defined(__sparc64__) || \
    defined(__ia64) || defined(__ia64__) || defined(__itanium__) || defined(_M_IA64) || \
    defined(__LP64__) || defined(_LP64) || defined(__64BIT__)
#   if !(defined(MP_64BIT) || defined(MP_32BIT) || defined(MP_16BIT) || defined(MP_8BIT))
#      if defined(__GNUC__) && !defined(__hppa)
/* we support 128bit integers only via: __attribute__((mode(TI))) */
#         define MP_64BIT
#      else
/* otherwise we fall back to MP_32BIT even on 64bit platforms */
#         define MP_32BIT
#      endif
#   endif
#endif

#ifdef MP_DIGIT_BIT
#   error Defining MP_DIGIT_BIT is disallowed, use MP_8/16/31/32/64BIT
#endif

/* some default configurations.
 *
 * A "mp_digit" must be able to hold MP_DIGIT_BIT + 1 bits
 * A "mp_word" must be able to hold 2*MP_DIGIT_BIT + 1 bits
 *
 * At the very least a mp_digit must be able to hold 7 bits
 * [any size beyond that is ok provided it doesn't overflow the data type]
 */

#ifdef MP_8BIT
typedef uint8_t              mp_digit;
typedef uint16_t             private_mp_word;
#   define MP_DIGIT_BIT 7
#elif defined(MP_16BIT)
typedef uint16_t             mp_digit;
typedef uint32_t             private_mp_word;
#   define MP_DIGIT_BIT 15
#elif defined(MP_64BIT)
/* for GCC only on supported platforms */
typedef uint64_t mp_digit;
#if defined(__GNUC__)
typedef unsigned long        private_mp_word __attribute__((mode(TI)));
#endif
#   define MP_DIGIT_BIT 60
#else
typedef uint32_t             mp_digit;
typedef uint64_t             private_mp_word;
#   ifdef MP_31BIT
/*
 * This is an extension that uses 31-bit digits.
 * Please be aware that not all functions support this size, especially s_mp_mul_digs_fast
 * will be reduced to work on small numbers only:
 * Up to 8 limbs, 248 bits instead of up to 512 limbs, 15872 bits with MP_28BIT.
 */
#      define MP_DIGIT_BIT 31
#   else
/* default case is 28-bit digits, defines MP_28BIT as a handy macro to test */
#      define MP_DIGIT_BIT 28
#      define MP_28BIT
#   endif
#endif

/* mp_word is a private type */
#define mp_word MP_DEPRECATED_PRAGMA("mp_word has been made private") private_mp_word

#define MP_SIZEOF_MP_DIGIT (MP_DEPRECATED_PRAGMA("MP_SIZEOF_MP_DIGIT has been deprecated, use sizeof (mp_digit)") sizeof (mp_digit))

#define MP_MASK          ((((mp_digit)1)<<((mp_digit)MP_DIGIT_BIT))-((mp_digit)1))
#define MP_DIGIT_MAX     MP_MASK

/* Primality generation flags */
#define MP_PRIME_BBS      0x0001 /* BBS style prime */
#define MP_PRIME_SAFE     0x0002 /* Safe prime (p-1)/2 == prime */
#define MP_PRIME_2MSB_ON  0x0008 /* force 2nd MSB to 1 */

#define LTM_PRIME_BBS      (MP_DEPRECATED_PRAGMA("LTM_PRIME_BBS has been deprecated, use MP_PRIME_BBS") MP_PRIME_BBS)
#define LTM_PRIME_SAFE     (MP_DEPRECATED_PRAGMA("LTM_PRIME_SAFE has been deprecated, use MP_PRIME_SAFE") MP_PRIME_SAFE)
#define LTM_PRIME_2MSB_ON  (MP_DEPRECATED_PRAGMA("LTM_PRIME_2MSB_ON has been deprecated, use MP_PRIME_2MSB_ON") MP_PRIME_2MSB_ON)

#ifdef MP_USE_ENUMS
typedef enum {
   MP_ZPOS = 0,
   MP_NEG = 1
} mp_sign;
typedef enum {
   MP_LT = -1,
   MP_EQ = 0,
   MP_GT = 1
} mp_ord;
typedef enum {
   MP_NO = 0,
   MP_YES = 1
} mp_bool;
typedef enum {
   MP_OKAY  = 0,
   MP_ERR   = -1,
   MP_MEM   = -2,
   MP_VAL   = -3,
   MP_ITER  = -4
} mp_err;
#else
typedef int mp_sign;
#define MP_ZPOS       0   /* positive integer */
#define MP_NEG        1   /* negative */
typedef int mp_ord;
#define MP_LT        -1   /* less than */
#define MP_EQ         0   /* equal to */
#define MP_GT         1   /* greater than */
typedef int mp_bool;
#define MP_YES        1   /* yes response */
#define MP_NO         0   /* no response */
typedef int mp_err;
#define MP_OKAY       0   /* ok result */
#define MP_ERR        -1  /* unknown error */
#define MP_MEM        -2  /* out of mem */
#define MP_VAL        -3  /* invalid input */
#define MP_RANGE      (MP_DEPRECATED_PRAGMA("MP_RANGE has been deprecated in favor of MP_VAL") MP_VAL)
#define MP_ITER       -4  /* Max. iterations reached */
#endif

/* tunable cutoffs */

#ifndef MP_FIXED_CUTOFFS
extern int
KARATSUBA_MUL_CUTOFF,
KARATSUBA_SQR_CUTOFF,
TOOM_MUL_CUTOFF,
TOOM_SQR_CUTOFF;
#endif

/* define this to use lower memory usage routines (exptmods mostly) */
/* #define MP_LOW_MEM */

/* default precision */
#ifndef MP_PREC
#   ifndef MP_LOW_MEM
#      define PRIVATE_MP_PREC 32        /* default digits of precision */
#   elif defined(MP_8BIT)
#      define PRIVATE_MP_PREC 16        /* default digits of precision */
#   else
#      define PRIVATE_MP_PREC 8         /* default digits of precision */
#   endif
#   define MP_PREC (MP_DEPRECATED_PRAGMA("MP_PREC is an internal macro") PRIVATE_MP_PREC)
#endif

/* size of comba arrays, should be at least 2 * 2**(BITS_PER_WORD - BITS_PER_DIGIT*2) */
#define PRIVATE_MP_WARRAY (int)(1uLL << (((8 * sizeof(private_mp_word)) - (2 * MP_DIGIT_BIT)) + 1))
#define MP_WARRAY (MP_DEPRECATED_PRAGMA("MP_WARRAY is an internal macro") PRIVATE_MP_WARRAY)

#if defined(__GNUC__) && __GNUC__ >= 4
#   define MP_NULL_TERMINATED __attribute__((sentinel))
#else
#   define MP_NULL_TERMINATED
#endif

/*
 * MP_WUR - warn unused result
 * ---------------------------
 *
 * The result of functions annotated with MP_WUR must be
 * checked and cannot be ignored.
 *
 * Most functions in libtommath return an error code.
 * This error code must be checked in order to prevent crashes or invalid
 * results.
 *
 * If you still want to avoid the error checks for quick and dirty programs
 * without robustness guarantees, you can `#define MP_WUR` before including
 * tommath.h, disabling the warnings.
 */
#ifndef MP_WUR
#  if defined(__GNUC__) && __GNUC__ >= 4
#     define MP_WUR __attribute__((warn_unused_result))
#  else
#     define MP_WUR
#  endif
#endif

#if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 405)
#  define MP_DEPRECATED(x) __attribute__((deprecated("replaced by " #x)))
#  define PRIVATE_MP_DEPRECATED_PRAGMA(s) _Pragma(#s)
#  define MP_DEPRECATED_PRAGMA(s) PRIVATE_MP_DEPRECATED_PRAGMA(GCC warning s)
#elif defined(_MSC_VER) && _MSC_VER >= 1500
#  define MP_DEPRECATED(x) __declspec(deprecated("replaced by " #x))
#  define MP_DEPRECATED_PRAGMA(s) __pragma(message(s))
#else
#  define MP_DEPRECATED(s)
#  define MP_DEPRECATED_PRAGMA(s)
#endif

#define DIGIT_BIT   (MP_DEPRECATED_PRAGMA("DIGIT_BIT macro is deprecated, MP_DIGIT_BIT instead") MP_DIGIT_BIT)
#define USED(m)     (MP_DEPRECATED_PRAGMA("USED macro is deprecated, use z->used instead") (m)->used)
#define DIGIT(m, k) (MP_DEPRECATED_PRAGMA("DIGIT macro is deprecated, use z->dp instead") (m)->dp[(k)])
#define SIGN(m)     (MP_DEPRECATED_PRAGMA("SIGN macro is deprecated, use z->sign instead") (m)->sign)

/* the infamous mp_int structure */
typedef struct  {
   int used, alloc;
   mp_sign sign;
   mp_digit *dp;
} mp_int;

/* callback for mp_prime_random, should fill dst with random bytes and return how many read [upto len] */
typedef int private_mp_prime_callback(unsigned char *dst, int len, void *dat);
typedef private_mp_prime_callback MP_DEPRECATED(mp_rand_source) ltm_prime_callback;

/* error code to char* string */
const char *mp_error_to_string(mp_err code) MP_WUR;

/* ---> init and deinit bignum functions <--- */
/* init a bignum */
static mp_err mp_init(mp_int *a) MP_WUR;

/* free a bignum */
static void mp_clear(mp_int *a);

/* init a null terminated series of arguments */
mp_err mp_init_multi(mp_int *mp, ...) MP_NULL_TERMINATED MP_WUR;

/* clear a null terminated series of arguments */
void mp_clear_multi(mp_int *mp, ...) MP_NULL_TERMINATED;

/* exchange two ints */
static void mp_exch(mp_int *a, mp_int *b);

/* shrink ram required for a bignum */
mp_err mp_shrink(mp_int *a) MP_WUR;

/* grow an int to a given size */
static mp_err mp_grow(mp_int *a, int size) MP_WUR;

/* init to a given number of digits */
static mp_err mp_init_size(mp_int *a, int size) MP_WUR;

/* ---> Basic Manipulations <--- */
#define mp_iszero(a) (((a)->used == 0) ? MP_YES : MP_NO)
mp_bool mp_iseven(const mp_int *a) MP_WUR;
mp_bool mp_isodd(const mp_int *a) MP_WUR;
#define mp_isneg(a)  (((a)->sign != MP_ZPOS) ? MP_YES : MP_NO)

/* set to zero */
static void mp_zero(mp_int *a);

/* get and set doubles */
static double mp_get_double(const mp_int *a) MP_WUR;
static mp_err mp_set_double(mp_int *a, double b) MP_WUR;

/* get integer, set integer and init with integer (int32_t) */
int32_t mp_get_i32(const mp_int *a) MP_WUR;
void mp_set_i32(mp_int *a, int32_t b);
mp_err mp_init_i32(mp_int *a, int32_t b) MP_WUR;

/* get integer, set integer and init with integer, behaves like two complement for negative numbers (uint32_t) */
#define mp_get_u32(a) ((uint32_t)mp_get_i32(a))
void mp_set_u32(mp_int *a, uint32_t b);
mp_err mp_init_u32(mp_int *a, uint32_t b) MP_WUR;

/* get integer, set integer and init with integer (int64_t) */
static int64_t mp_get_i64(const mp_int *a) MP_WUR;
static void mp_set_i64(mp_int *a, int64_t b);
mp_err mp_init_i64(mp_int *a, int64_t b) MP_WUR;

/* get integer, set integer and init with integer, behaves like two complement for negative numbers (uint64_t) */
#define mp_get_u64(a) ((uint64_t)mp_get_i64(a))
static void mp_set_u64(mp_int *a, uint64_t b);
mp_err mp_init_u64(mp_int *a, uint64_t b) MP_WUR;

/* get magnitude */
uint32_t mp_get_mag32(const mp_int *a) MP_WUR;
static uint64_t mp_get_mag64(const mp_int *a) MP_WUR;

/* get integer, set integer (long) */
#define mp_get_l(a)        (sizeof (long) == 8 ? (long)mp_get_i64(a) : (long)mp_get_i32(a))
#define mp_set_l(a, b)     (sizeof (long) == 8 ? mp_set_i64((a), (b)) : mp_set_i32((a), (int32_t)(b)))

/* get integer, set integer (unsigned long) */
#define mp_get_ul(a)       (sizeof (long) == 8 ? (unsigned long)mp_get_u64(a) : (unsigned long)mp_get_u32(a))
#define mp_set_ul(a, b)    (sizeof (long) == 8 ? mp_set_u64((a), (b)) : mp_set_u32((a), (uint32_t)(b)))
#define mp_get_magl(a)     (sizeof (long) == 8 ? (unsigned long)mp_get_mag64(a) : (unsigned long)mp_get_mag32(a))

/* set to single unsigned digit, up to MP_DIGIT_MAX */
void mp_set(mp_int *a, mp_digit b);
mp_err mp_init_set(mp_int *a, mp_digit b) MP_WUR;

/* get integer, set integer and init with integer (deprecated) */
MP_DEPRECATED(mp_get_mag32/mp_get_u32) unsigned long mp_get_int(const mp_int *a) MP_WUR;
MP_DEPRECATED(mp_get_magl/mp_get_ul) unsigned long mp_get_long(const mp_int *a) MP_WUR;
MP_DEPRECATED(mp_get_mag64/mp_get_u64) unsigned long long mp_get_long_long(const mp_int *a) MP_WUR;
MP_DEPRECATED(mp_set_u32) mp_err mp_set_int(mp_int *a, unsigned long b);
MP_DEPRECATED(mp_set_ul) mp_err mp_set_long(mp_int *a, unsigned long b);
MP_DEPRECATED(mp_set_u64) mp_err mp_set_long_long(mp_int *a, unsigned long long b);
MP_DEPRECATED(mp_init_u32) mp_err mp_init_set_int(mp_int *a, unsigned long b) MP_WUR;

/* copy, b = a */
static mp_err mp_copy(const mp_int *a, mp_int *b) MP_WUR;

/* inits and copies, a = b */
static mp_err mp_init_copy(mp_int *a, const mp_int *b) MP_WUR;

/* trim unused digits */
static void mp_clamp(mp_int *a);

/* import binary data */
mp_err mp_import(mp_int *rop, size_t count, int order, size_t size, int endian, size_t nails, const void *op) MP_WUR;

/* export binary data */
mp_err mp_export(void *rop, size_t *countp, int order, size_t size, int endian, size_t nails, const mp_int *op) MP_WUR;

/* ---> digit manipulation <--- */

/* right shift by "b" digits */
static void mp_rshd(mp_int *a, int b);

/* left shift by "b" digits */
static mp_err mp_lshd(mp_int *a, int b) MP_WUR;

/* c = a / 2**b, implemented as c = a >> b */
static mp_err mp_div_2d(const mp_int *a, int b, mp_int *c, mp_int *d) MP_WUR;

/* b = a/2 */
mp_err mp_div_2(const mp_int *a, mp_int *b) MP_WUR;

/* a/3 => 3c + d == a */
mp_err mp_div_3(const mp_int *a, mp_int *c, mp_digit *d) MP_WUR;

/* c = a * 2**b, implemented as c = a << b */
static mp_err mp_mul_2d(const mp_int *a, int b, mp_int *c) MP_WUR;

/* b = a*2 */
mp_err mp_mul_2(const mp_int *a, mp_int *b) MP_WUR;

/* c = a mod 2**b */
static mp_err mp_mod_2d(const mp_int *a, int b, mp_int *c) MP_WUR;

/* computes a = 2**b */
mp_err mp_2expt(mp_int *a, int b) MP_WUR;

/* Counts the number of lsbs which are zero before the first zero bit */
int mp_cnt_lsb(const mp_int *a) MP_WUR;

/* I Love Earth! */

/* makes a pseudo-random mp_int of a given size */
mp_err mp_rand(mp_int *a, int digits) MP_WUR;
/* makes a pseudo-random small int of a given size */
MP_DEPRECATED(mp_rand) mp_err mp_rand_digit(mp_digit *r) MP_WUR;
/* use custom random data source instead of source provided the platform */
void mp_rand_source(mp_err(*source)(void *out, size_t size));

#ifdef MP_PRNG_ENABLE_LTM_RNG
#  warning MP_PRNG_ENABLE_LTM_RNG has been deprecated, use mp_rand_source instead.
/* A last resort to provide random data on systems without any of the other
 * implemented ways to gather entropy.
 * It is compatible with `rng_get_bytes()` from libtomcrypt so you could
 * provide that one and then set `ltm_rng = rng_get_bytes;` */
extern unsigned long (*ltm_rng)(unsigned char *out, unsigned long outlen, void (*callback)(void));
extern void (*ltm_rng_callback)(void);
#endif

/* ---> binary operations <--- */

/* Checks the bit at position b and returns MP_YES
 * if the bit is 1, MP_NO if it is 0 and MP_VAL
 * in case of error
 */
MP_DEPRECATED(s_mp_get_bit) int mp_get_bit(const mp_int *a, int b) MP_WUR;

/* c = a XOR b (two complement) */
MP_DEPRECATED(mp_xor) mp_err mp_tc_xor(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
static mp_err mp_xor(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* c = a OR b (two complement) */
MP_DEPRECATED(mp_or) mp_err mp_tc_or(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
static mp_err mp_or(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* c = a AND b (two complement) */
MP_DEPRECATED(mp_and) mp_err mp_tc_and(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
static mp_err mp_and(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* b = ~a (bitwise not, two complement) */
static mp_err mp_complement(const mp_int *a, mp_int *b) MP_WUR;

/* right shift with sign extension */
MP_DEPRECATED(mp_signed_rsh) mp_err mp_tc_div_2d(const mp_int *a, int b, mp_int *c) MP_WUR;
static mp_err mp_signed_rsh(const mp_int *a, int b, mp_int *c) MP_WUR;

/* ---> Basic arithmetic <--- */

/* b = -a */
static mp_err mp_neg(const mp_int *a, mp_int *b) MP_WUR;

/* b = |a| */
mp_err mp_abs(const mp_int *a, mp_int *b) MP_WUR;

/* compare a to b */
static mp_ord mp_cmp(const mp_int *a, const mp_int *b) MP_WUR;

/* compare |a| to |b| */
static mp_ord mp_cmp_mag(const mp_int *a, const mp_int *b) MP_WUR;

/* c = a + b */
static mp_err mp_add(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* c = a - b */
static mp_err mp_sub(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* c = a * b */
static mp_err mp_mul(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* b = a*a  */
mp_err mp_sqr(const mp_int *a, mp_int *b) MP_WUR;

/* a/b => cb + d == a */
static mp_err mp_div(const mp_int *a, const mp_int *b, mp_int *c, mp_int *d) MP_WUR;

/* c = a mod b, 0 <= c < b  */
static mp_err mp_mod(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* Increment "a" by one like "a++". Changes input! */
mp_err mp_incr(mp_int *a) MP_WUR;

/* Decrement "a" by one like "a--". Changes input! */
mp_err mp_decr(mp_int *a) MP_WUR;

/* ---> single digit functions <--- */

/* compare against a single digit */
mp_ord mp_cmp_d(const mp_int *a, mp_digit b) MP_WUR;

/* c = a + b */
static mp_err mp_add_d(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;

/* c = a - b */
static mp_err mp_sub_d(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;

/* c = a * b */
static mp_err mp_mul_d(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;

/* a/b => cb + d == a */
static mp_err mp_div_d(const mp_int *a, mp_digit b, mp_int *c, mp_digit *d) MP_WUR;

/* c = a mod b, 0 <= c < b  */
mp_err mp_mod_d(const mp_int *a, mp_digit b, mp_digit *c) MP_WUR;

/* ---> number theory <--- */

/* d = a + b (mod c) */
mp_err mp_addmod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d) MP_WUR;

/* d = a - b (mod c) */
mp_err mp_submod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d) MP_WUR;

/* d = a * b (mod c) */
mp_err mp_mulmod(const mp_int *a, const mp_int *b, const mp_int *c, mp_int *d) MP_WUR;

/* c = a * a (mod b) */
mp_err mp_sqrmod(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* c = 1/a (mod b) */
mp_err mp_invmod(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* c = (a, b) */
mp_err mp_gcd(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* produces value such that U1*a + U2*b = U3 */
mp_err mp_exteuclid(const mp_int *a, const mp_int *b, mp_int *U1, mp_int *U2, mp_int *U3) MP_WUR;

/* c = [a, b] or (a*b)/(a, b) */
mp_err mp_lcm(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;

/* finds one of the b'th root of a, such that |c|**b <= |a|
 *
 * returns error if a < 0 and b is even
 */
mp_err mp_root(const mp_int *a, uint32_t b, mp_int *c) MP_WUR;
MP_DEPRECATED(mp_root) mp_err mp_n_root(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;
MP_DEPRECATED(mp_n_root_ex) mp_err mp_n_root_ex(const mp_int *a, mp_digit b, mp_int *c, int fast) MP_WUR;

/* special sqrt algo */
mp_err mp_sqrt(const mp_int *arg, mp_int *ret) MP_WUR;

/* special sqrt (mod prime) */
mp_err mp_sqrtmod_prime(const mp_int *n, const mp_int *prime, mp_int *ret) MP_WUR;

/* is number a square? */
mp_err mp_is_square(const mp_int *arg, mp_bool *ret) MP_WUR;

/* computes the jacobi c = (a | n) (or Legendre if b is prime)  */
MP_DEPRECATED(mp_kronecker) mp_err mp_jacobi(const mp_int *a, const mp_int *n, int *c) MP_WUR;

/* computes the Kronecker symbol c = (a | p) (like jacobi() but with {a,p} in Z */
mp_err mp_kronecker(const mp_int *a, const mp_int *p, int *c) MP_WUR;

/* used to setup the Barrett reduction for a given modulus b */
mp_err mp_reduce_setup(mp_int *a, const mp_int *b) MP_WUR;

/* Barrett Reduction, computes a (mod b) with a precomputed value c
 *
 * Assumes that 0 < x <= m*m, note if 0 > x > -(m*m) then you can merely
 * compute the reduction as -1 * mp_reduce(mp_abs(x)) [pseudo code].
 */
mp_err mp_reduce(mp_int *x, const mp_int *m, const mp_int *mu) MP_WUR;

/* setups the montgomery reduction */
mp_err mp_montgomery_setup(const mp_int *n, mp_digit *rho) MP_WUR;

/* computes a = B**n mod b without division or multiplication useful for
 * normalizing numbers in a Montgomery system.
 */
mp_err mp_montgomery_calc_normalization(mp_int *a, const mp_int *b) MP_WUR;

/* computes x/R == x (mod N) via Montgomery Reduction */
mp_err mp_montgomery_reduce(mp_int *x, const mp_int *n, mp_digit rho) MP_WUR;

/* returns 1 if a is a valid DR modulus */
mp_bool mp_dr_is_modulus(const mp_int *a) MP_WUR;

/* sets the value of "d" required for mp_dr_reduce */
void mp_dr_setup(const mp_int *a, mp_digit *d);

/* reduces a modulo n using the Diminished Radix method */
mp_err mp_dr_reduce(mp_int *x, const mp_int *n, mp_digit k) MP_WUR;

/* returns true if a can be reduced with mp_reduce_2k */
mp_bool mp_reduce_is_2k(const mp_int *a) MP_WUR;

/* determines k value for 2k reduction */
mp_err mp_reduce_2k_setup(const mp_int *a, mp_digit *d) MP_WUR;

/* reduces a modulo b where b is of the form 2**p - k [0 <= a] */
mp_err mp_reduce_2k(mp_int *a, const mp_int *n, mp_digit d) MP_WUR;

/* returns true if a can be reduced with mp_reduce_2k_l */
mp_bool mp_reduce_is_2k_l(const mp_int *a) MP_WUR;

/* determines k value for 2k reduction */
mp_err mp_reduce_2k_setup_l(const mp_int *a, mp_int *d) MP_WUR;

/* reduces a modulo b where b is of the form 2**p - k [0 <= a] */
mp_err mp_reduce_2k_l(mp_int *a, const mp_int *n, const mp_int *d) MP_WUR;

/* Y = G**X (mod P) */
mp_err mp_exptmod(const mp_int *G, const mp_int *X, const mp_int *P, mp_int *Y) MP_WUR;

/* ---> Primes <--- */

/* number of primes */
#ifdef MP_8BIT
#  define PRIVATE_MP_PRIME_TAB_SIZE 31
#else
#  define PRIVATE_MP_PRIME_TAB_SIZE 256
#endif
#define PRIME_SIZE (MP_DEPRECATED_PRAGMA("PRIME_SIZE has been made internal") PRIVATE_MP_PRIME_TAB_SIZE)

/* table of first PRIME_SIZE primes */
MP_DEPRECATED(internal) extern const mp_digit ltm_prime_tab[PRIVATE_MP_PRIME_TAB_SIZE];

/* result=1 if a is divisible by one of the first PRIME_SIZE primes */
MP_DEPRECATED(mp_prime_is_prime) mp_err mp_prime_is_divisible(const mp_int *a, mp_bool *result) MP_WUR;

/* performs one Fermat test of "a" using base "b".
 * Sets result to 0 if composite or 1 if probable prime
 */
mp_err mp_prime_fermat(const mp_int *a, const mp_int *b, mp_bool *result) MP_WUR;

/* performs one Miller-Rabin test of "a" using base "b".
 * Sets result to 0 if composite or 1 if probable prime
 */
mp_err mp_prime_miller_rabin(const mp_int *a, const mp_int *b, mp_bool *result) MP_WUR;

/* This gives [for a given bit size] the number of trials required
 * such that Miller-Rabin gives a prob of failure lower than 2^-96
 */
int mp_prime_rabin_miller_trials(int size) MP_WUR;

/* performs one strong Lucas-Selfridge test of "a".
 * Sets result to 0 if composite or 1 if probable prime
 */
mp_err mp_prime_strong_lucas_selfridge(const mp_int *a, mp_bool *result) MP_WUR;

/* performs one Frobenius test of "a" as described by Paul Underwood.
 * Sets result to 0 if composite or 1 if probable prime
 */
mp_err mp_prime_frobenius_underwood(const mp_int *N, mp_bool *result) MP_WUR;

/* performs t random rounds of Miller-Rabin on "a" additional to
 * bases 2 and 3.  Also performs an initial sieve of trial
 * division.  Determines if "a" is prime with probability
 * of error no more than (1/4)**t.
 * Both a strong Lucas-Selfridge to complete the BPSW test
 * and a separate Frobenius test are available at compile time.
 * With t<0 a deterministic test is run for primes up to
 * 318665857834031151167461. With t<13 (abs(t)-13) additional
 * tests with sequential small primes are run starting at 43.
 * Is Fips 186.4 compliant if called with t as computed by
 * mp_prime_rabin_miller_trials();
 *
 * Sets result to 1 if probably prime, 0 otherwise
 */
mp_err mp_prime_is_prime(const mp_int *a, int t, mp_bool *result) MP_WUR;

/* finds the next prime after the number "a" using "t" trials
 * of Miller-Rabin.
 *
 * bbs_style = 1 means the prime must be congruent to 3 mod 4
 */
mp_err mp_prime_next_prime(mp_int *a, int t, int bbs_style) MP_WUR;

/* makes a truly random prime of a given size (bytes),
 * call with bbs = 1 if you want it to be congruent to 3 mod 4
 *
 * You have to supply a callback which fills in a buffer with random bytes.  "dat" is a parameter you can
 * have passed to the callback (e.g. a state or something).  This function doesn't use "dat" itself
 * so it can be NULL
 *
 * The prime generated will be larger than 2^(8*size).
 */
#define mp_prime_random(a, t, size, bbs, cb, dat) (MP_DEPRECATED_PRAGMA("mp_prime_random has been deprecated, use mp_prime_rand instead") mp_prime_random_ex(a, t, ((size) * 8) + 1, (bbs==1)?MP_PRIME_BBS:0, cb, dat))

/* makes a truly random prime of a given size (bits),
 *
 * Flags are as follows:
 *
 *   MP_PRIME_BBS      - make prime congruent to 3 mod 4
 *   MP_PRIME_SAFE     - make sure (p-1)/2 is prime as well (implies MP_PRIME_BBS)
 *   MP_PRIME_2MSB_ON  - make the 2nd highest bit one
 *
 * You have to supply a callback which fills in a buffer with random bytes.  "dat" is a parameter you can
 * have passed to the callback (e.g. a state or something).  This function doesn't use "dat" itself
 * so it can be NULL
 *
 */
MP_DEPRECATED(mp_prime_rand) mp_err mp_prime_random_ex(mp_int *a, int t, int size, int flags,
      private_mp_prime_callback cb, void *dat) MP_WUR;
mp_err mp_prime_rand(mp_int *a, int t, int size, int flags) MP_WUR;

/* Integer logarithm to integer base */
mp_err mp_ilogb(const mp_int *a, uint32_t base, mp_int *c) MP_WUR;

/* c = a**b */
mp_err mp_expt(const mp_int *a, uint32_t b, mp_int *c) MP_WUR;
MP_DEPRECATED(mp_exp) mp_err mp_expt_d(const mp_int *a, mp_digit b, mp_int *c) MP_WUR;
MP_DEPRECATED(mp_expt_d) mp_err mp_expt_d_ex(const mp_int *a, mp_digit b, mp_int *c, int fast) MP_WUR;

/* ---> radix conversion <--- */
static int mp_count_bits(const mp_int *a) MP_WUR;

int mp_unsigned_bin_size(const mp_int *a) MP_WUR;
static mp_err mp_read_unsigned_bin(mp_int *a, const unsigned char *b, int c) MP_WUR;
mp_err mp_to_unsigned_bin(const mp_int *a, unsigned char *b) MP_WUR;
mp_err mp_to_unsigned_bin_n(const mp_int *a, unsigned char *b, unsigned long *outlen) MP_WUR;

int mp_signed_bin_size(const mp_int *a) MP_WUR;
mp_err mp_read_signed_bin(mp_int *a, const unsigned char *b, int c) MP_WUR;
mp_err mp_to_signed_bin(const mp_int *a,  unsigned char *b) MP_WUR;
mp_err mp_to_signed_bin_n(const mp_int *a, unsigned char *b, unsigned long *outlen) MP_WUR;

mp_err mp_read_radix(mp_int *a, const char *str, int radix) MP_WUR;
static mp_err mp_toradix(const mp_int *a, char *str, int radix) MP_WUR;
mp_err mp_toradix_n(const mp_int *a, char *str, int radix, int maxlen) MP_WUR;
static mp_err mp_radix_size(const mp_int *a, int radix, int *size) MP_WUR;

#ifndef MP_NO_FILE
mp_err mp_fread(mp_int *a, int radix, FILE *stream) MP_WUR;
mp_err mp_fwrite(const mp_int *a, int radix, FILE *stream) MP_WUR;
#endif

#define mp_read_raw(mp, str, len) (MP_DEPRECATED_PRAGMA("replaced by mp_read_signed_bin") mp_read_signed_bin((mp), (str), (len)))
#define mp_raw_size(mp)           (MP_DEPRECATED_PRAGMA("replaced by mp_signed_bin_size") mp_signed_bin_size(mp))
#define mp_toraw(mp, str)         (MP_DEPRECATED_PRAGMA("replaced by mp_to_signed_bin") mp_to_signed_bin((mp), (str)))
#define mp_read_mag(mp, str, len) (MP_DEPRECATED_PRAGMA("replaced by mp_read_unsigned_bin") mp_read_unsigned_bin((mp), (str), (len))
#define mp_mag_size(mp)           (MP_DEPRECATED_PRAGMA("replaced by mp_unsigned_bin_size") mp_unsigned_bin_size(mp))
#define mp_tomag(mp, str)         (MP_DEPRECATED_PRAGMA("replaced by mp_to_unsigned_bin") mp_to_unsigned_bin((mp), (str)))

#define mp_tobinary(M, S)  mp_toradix((M), (S), 2)
#define mp_tooctal(M, S)   mp_toradix((M), (S), 8)
#define mp_todecimal(M, S) mp_toradix((M), (S), 10)
#define mp_tohex(M, S)     mp_toradix((M), (S), 16)

#ifdef __cplusplus
}
#endif

#endif
/*
   Current values evaluated on an AMD A8-6600K (64-bit).
   Type "make tune" to optimize them for your machine but
   be aware that it may take a long time. It took 2:30 minutes
   on the aforementioned machine for example.
 */

#define MP_DEFAULT_KARATSUBA_MUL_CUTOFF 80
#define MP_DEFAULT_KARATSUBA_SQR_CUTOFF 120
#define MP_DEFAULT_TOOM_MUL_CUTOFF      350
#define MP_DEFAULT_TOOM_SQR_CUTOFF      400
#ifndef TOMMATH_PRIVATE_H_
#define TOMMATH_PRIVATE_H_

/*
 * Private symbols
 * ---------------
 *
 * On Unix symbols can be marked as hidden if libtommath is compiled
 * as a shared object. By default, symbols are visible.
 * As of now, this feature is opt-in via the MP_PRIVATE_SYMBOLS define.
 *
 * On Win32 a .def file must be used to specify the exported symbols.
 */
#if defined (MP_PRIVATE_SYMBOLS) && defined(__GNUC__) && __GNUC__ >= 4
#   define MP_PRIVATE __attribute__ ((visibility ("hidden")))
#else
#   define MP_PRIVATE
#endif

/* Hardening libtommath
 * --------------------
 *
 * By default memory is zeroed before calling
 * MP_FREE to avoid leaking data. This is good
 * practice in cryptographical applications.
 *
 * Note however that memory allocators used
 * in cryptographical applications can often
 * be configured by itself to clear memory,
 * rendering the clearing in tommath unnecessary.
 * See for example https://github.com/GrapheneOS/hardened_malloc
 * and the option CONFIG_ZERO_ON_FREE.
 *
 * Furthermore there are applications which
 * value performance more and want this
 * feature to be disabled. For such applications
 * define MP_NO_ZERO_ON_FREE during compilation.
 */
#ifdef MP_NO_ZERO_ON_FREE
#  define MP_FREE_BUFFER(mem, size)   MP_FREE((mem), (size))
#  define MP_FREE_DIGITS(mem, digits) MP_FREE((mem), sizeof (mp_digit) * (size_t)(digits))
#else
#  define MP_FREE_BUFFER(mem, size)                     \
do {                                                    \
   size_t fs_ = (size);                                 \
   void* fm_ = (mem);                                   \
   if (fm_ != NULL) {                                   \
      MP_ZERO_BUFFER(fm_, fs_);                         \
      MP_FREE(fm_, fs_);                                \
   }                                                    \
} while (0)
#  define MP_FREE_DIGITS(mem, digits)                   \
do {                                                    \
   int fd_ = (digits);                                  \
   void* fm_ = (mem);                                   \
   if (fm_ != NULL) {                                   \
      size_t fs_ = sizeof (mp_digit) * (size_t)fd_;     \
      MP_ZERO_BUFFER(fm_, fs_);                         \
      MP_FREE(fm_, fs_);                                \
   }                                                    \
} while (0)
#endif

#ifdef MP_USE_MEMSET
#  include <string.h>
#  define MP_ZERO_BUFFER(mem, size)   memset((mem), 0, (size))
#  define MP_ZERO_DIGITS(mem, digits)                   \
do {                                                    \
   int zd_ = (digits);                                  \
   if (zd_ > 0) {                                       \
      memset((mem), 0, sizeof(mp_digit) * (size_t)zd_); \
   }                                                    \
} while (0)
#else
#  define MP_ZERO_BUFFER(mem, size)                     \
do {                                                    \
   size_t zs_ = (size);                                 \
   char* zm_ = (char*)(mem);                            \
   while (zs_-- > 0u) {                                 \
      *zm_++ = '\0';                                    \
   }                                                    \
} while (0)
#  define MP_ZERO_DIGITS(mem, digits)                   \
do {                                                    \
   int zd_ = (digits);                                  \
   mp_digit* zm_ = (mem);                               \
   while (zd_-- > 0) {                                  \
      *zm_++ = 0;                                       \
   }                                                    \
} while (0)
#endif

/* Tunable cutoffs
 * ---------------
 *
 *  - In the default settings, a cutoff X can be modified at runtime
 *    by adjusting the corresponding X_CUTOFF variable.
 *
 *  - Tunability of the library can be disabled at compile time
 *    by defining the MP_FIXED_CUTOFFS macro.
 *
 *  - There is an additional file tommath_cutoffs.h, which defines
 *    the default cutoffs. These can be adjusted manually or by the
 *    autotuner.
 *
 */

#ifdef MP_FIXED_CUTOFFS
#  define MP_KARATSUBA_MUL_CUTOFF MP_DEFAULT_KARATSUBA_MUL_CUTOFF
#  define MP_KARATSUBA_SQR_CUTOFF MP_DEFAULT_KARATSUBA_SQR_CUTOFF
#  define MP_TOOM_MUL_CUTOFF      MP_DEFAULT_TOOM_MUL_CUTOFF
#  define MP_TOOM_SQR_CUTOFF      MP_DEFAULT_TOOM_SQR_CUTOFF
#else
#  define MP_KARATSUBA_MUL_CUTOFF KARATSUBA_MUL_CUTOFF
#  define MP_KARATSUBA_SQR_CUTOFF KARATSUBA_SQR_CUTOFF
#  define MP_TOOM_MUL_CUTOFF      TOOM_MUL_CUTOFF
#  define MP_TOOM_SQR_CUTOFF      TOOM_SQR_CUTOFF
#endif

/* feature detection macro */
#define MP_STRINGIZE(x)  MP__STRINGIZE(x)
#define MP__STRINGIZE(x) ""#x""
#define MP_HAS(x)        (sizeof(MP_STRINGIZE(x##_C)) == 1)

/* TODO: Remove private_mp_word as soon as deprecated mp_word is removed from tommath. */
#undef mp_word
typedef private_mp_word mp_word;

#define MP_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MP_MAX(x, y) (((x) > (y)) ? (x) : (y))

/* Static assertion */
#define MP_STATIC_ASSERT(msg, cond) typedef char mp_static_assert_##msg[(cond) ? 1 : -1];

/* ---> Basic Manipulations <--- */
#define MP_IS_ZERO(a) ((a)->used == 0)
#define MP_IS_EVEN(a) (((a)->used == 0) || (((a)->dp[0] & 1u) == 0u))
#define MP_IS_ODD(a)  (((a)->used > 0) && (((a)->dp[0] & 1u) == 1u))

#define MP_SIZEOF_BITS(type)    ((size_t)8 * sizeof(type))
#define MP_MAXFAST              (int)(1uL << (MP_SIZEOF_BITS(mp_word) - (2u * (size_t)MP_DIGIT_BIT)))

/* TODO: Remove PRIVATE_MP_WARRAY as soon as deprecated MP_WARRAY is removed from tommath.h */
#undef MP_WARRAY
#define MP_WARRAY PRIVATE_MP_WARRAY

/* TODO: Remove PRIVATE_MP_PREC as soon as deprecated MP_PREC is removed from tommath.h */
#ifdef PRIVATE_MP_PREC
#   undef MP_PREC
#   define MP_PREC PRIVATE_MP_PREC
#endif

/* Minimum number of available digits in mp_int, MP_PREC >= MP_MIN_PREC */
#define MP_MIN_PREC ((((int)MP_SIZEOF_BITS(long long) + MP_DIGIT_BIT) - 1) / MP_DIGIT_BIT)

MP_STATIC_ASSERT(prec_geq_min_prec, MP_PREC >= MP_MIN_PREC)

/* random number source */
extern MP_PRIVATE mp_err(*s_mp_rand_source)(void *out, size_t size);

/* lowlevel functions, do not call! */
MP_PRIVATE mp_bool s_mp_get_bit(const mp_int *a, unsigned int b);
static MP_PRIVATE mp_err s_mp_add(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
static MP_PRIVATE mp_err s_mp_sub(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
static MP_PRIVATE mp_err s_mp_mul_digs_fast(const mp_int *a, const mp_int *b, mp_int *c, int digs) MP_WUR;
static MP_PRIVATE mp_err s_mp_mul_digs(const mp_int *a, const mp_int *b, mp_int *c, int digs) MP_WUR;
MP_PRIVATE mp_err s_mp_mul_high_digs_fast(const mp_int *a, const mp_int *b, mp_int *c, int digs) MP_WUR;
MP_PRIVATE mp_err s_mp_mul_high_digs(const mp_int *a, const mp_int *b, mp_int *c, int digs) MP_WUR;
MP_PRIVATE mp_err s_mp_sqr_fast(const mp_int *a, mp_int *b) MP_WUR;
MP_PRIVATE mp_err s_mp_sqr(const mp_int *a, mp_int *b) MP_WUR;
MP_PRIVATE mp_err s_mp_balance_mul(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
static MP_PRIVATE mp_err s_mp_karatsuba_mul(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
MP_PRIVATE mp_err s_mp_toom_mul(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
MP_PRIVATE mp_err s_mp_karatsuba_sqr(const mp_int *a, mp_int *b) MP_WUR;
MP_PRIVATE mp_err s_mp_toom_sqr(const mp_int *a, mp_int *b) MP_WUR;
MP_PRIVATE mp_err s_mp_invmod_fast(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
MP_PRIVATE mp_err s_mp_invmod_slow(const mp_int *a, const mp_int *b, mp_int *c) MP_WUR;
MP_PRIVATE mp_err s_mp_montgomery_reduce_fast(mp_int *x, const mp_int *n, mp_digit rho) MP_WUR;
MP_PRIVATE mp_err s_mp_exptmod_fast(const mp_int *G, const mp_int *X, const mp_int *P, mp_int *Y, int redmode) MP_WUR;
MP_PRIVATE mp_err s_mp_exptmod(const mp_int *G, const mp_int *X, const mp_int *P, mp_int *Y, int redmode) MP_WUR;
MP_PRIVATE mp_err s_mp_rand_platform(void *p, size_t n) MP_WUR;
MP_PRIVATE mp_err s_mp_prime_random_ex(mp_int *a, int t, int size, int flags, private_mp_prime_callback cb, void *dat);
static MP_PRIVATE void s_mp_reverse(unsigned char *s, int len);
MP_PRIVATE mp_err s_mp_prime_is_divisible(const mp_int *a, mp_bool *result);

/* TODO: jenkins prng is not thread safe as of now */
MP_PRIVATE mp_err s_mp_rand_jenkins(void *p, size_t n) MP_WUR;
MP_PRIVATE void s_mp_rand_jenkins_init(uint64_t seed);

extern MP_PRIVATE const char *const mp_s_rmap;
extern MP_PRIVATE const uint8_t mp_s_rmap_reverse[];
extern MP_PRIVATE const size_t mp_s_rmap_reverse_sz;
extern MP_PRIVATE const mp_digit *s_mp_prime_tab;

/* deprecated functions */
MP_DEPRECATED(s_mp_invmod_fast) mp_err fast_mp_invmod(const mp_int *a, const mp_int *b, mp_int *c);
MP_DEPRECATED(s_mp_montgomery_reduce_fast) mp_err fast_mp_montgomery_reduce(mp_int *x, const mp_int *n,
      mp_digit rho);
MP_DEPRECATED(s_mp_mul_digs_fast) mp_err fast_s_mp_mul_digs(const mp_int *a, const mp_int *b, mp_int *c,
      int digs);
MP_DEPRECATED(s_mp_mul_high_digs_fast) mp_err fast_s_mp_mul_high_digs(const mp_int *a, const mp_int *b,
      mp_int *c,
      int digs);
MP_DEPRECATED(s_mp_sqr_fast) mp_err fast_s_mp_sqr(const mp_int *a, mp_int *b);
MP_DEPRECATED(s_mp_balance_mul) mp_err mp_balance_mul(const mp_int *a, const mp_int *b, mp_int *c);
MP_DEPRECATED(s_mp_exptmod_fast) mp_err mp_exptmod_fast(const mp_int *G, const mp_int *X, const mp_int *P,
      mp_int *Y,
      int redmode);
MP_DEPRECATED(s_mp_invmod_slow) mp_err mp_invmod_slow(const mp_int *a, const mp_int *b, mp_int *c);
MP_DEPRECATED(s_mp_karatsuba_mul) mp_err mp_karatsuba_mul(const mp_int *a, const mp_int *b, mp_int *c);
MP_DEPRECATED(s_mp_karatsuba_sqr) mp_err mp_karatsuba_sqr(const mp_int *a, mp_int *b);
MP_DEPRECATED(s_mp_toom_mul) mp_err mp_toom_mul(const mp_int *a, const mp_int *b, mp_int *c);
MP_DEPRECATED(s_mp_toom_sqr) mp_err mp_toom_sqr(const mp_int *a, mp_int *b);
MP_DEPRECATED(s_mp_reverse) void bn_reverse(unsigned char *s, int len);

/* code-generating macros */
#define MP_SET_UNSIGNED(name, type)                                                    \
    void name(mp_int * a, type b)                                                      \
    {                                                                                  \
        int i = 0;                                                                     \
        while (b != 0u) {                                                              \
            a->dp[i++] = ((mp_digit)b & MP_MASK);                                      \
            if (MP_SIZEOF_BITS(type) <= MP_DIGIT_BIT) { break; }                       \
            b >>= ((MP_SIZEOF_BITS(type) <= MP_DIGIT_BIT) ? 0 : MP_DIGIT_BIT);         \
        }                                                                              \
        a->used = i;                                                                   \
        a->sign = MP_ZPOS;                                                             \
        MP_ZERO_DIGITS(a->dp + a->used, a->alloc - a->used);                           \
    }

#define MP_SET_SIGNED(name, uname, type, utype)          \
    void name(mp_int * a, type b)                        \
    {                                                    \
        uname(a, (b < 0) ? -(utype)b : (utype)b);        \
        if (b < 0) { a->sign = MP_NEG; }                 \
    }

#define MP_INIT_INT(name , set, type)                    \
    mp_err name(mp_int * a, type b)                      \
    {                                                    \
        mp_err err;                                      \
        if ((err = mp_init(a)) != MP_OKAY) {             \
            return err;                                  \
        }                                                \
        set(a, b);                                       \
        return MP_OKAY;                                  \
    }

#define MP_GET_MAG(type, name)                                                         \
    type name(const mp_int* a)                                                         \
    {                                                                                  \
        unsigned i = MP_MIN((unsigned)a->used, (unsigned)((MP_SIZEOF_BITS(type) + MP_DIGIT_BIT - 1) / MP_DIGIT_BIT)); \
        type res = 0u;                                                                 \
        while (i --> 0u) {                                                             \
            res <<= ((MP_SIZEOF_BITS(type) <= MP_DIGIT_BIT) ? 0 : MP_DIGIT_BIT);       \
            res |= (type)a->dp[i];                                                     \
            if (MP_SIZEOF_BITS(type) <= MP_DIGIT_BIT) { break; }                       \
        }                                                                              \
        return res;                                                                    \
    }

#define MP_GET_SIGNED(type, name, mag)                        \
    type name(const mp_int* a)                                \
    {                                                         \
        uint64_t res = mag(a);                                \
        return (a->sign == MP_NEG) ? (type)-res : (type)res;  \
    }

#endif
#ifdef MP_ADD_C
/* high level addition (handles signs) */
mp_err mp_add(const mp_int *a, const mp_int *b, mp_int *c)
{
   mp_sign sa, sb;
   mp_err err;

   /* get sign of both inputs */
   sa = a->sign;
   sb = b->sign;

   /* handle two cases, not four */
   if (sa == sb) {
      /* both positive or both negative */
      /* add their magnitudes, copy the sign */
      c->sign = sa;
      err = s_mp_add(a, b, c);
   } else {
      /* one positive, the other negative */
      /* subtract the one with the greater magnitude from */
      /* the one of the lesser magnitude.  The result gets */
      /* the sign of the one with the greater magnitude. */
      if (mp_cmp_mag(a, b) == MP_LT) {
         c->sign = sb;
         err = s_mp_sub(b, a, c);
      } else {
         c->sign = sa;
         err = s_mp_sub(a, b, c);
      }
   }
   return err;
}

#endif
#ifdef MP_ADD_D_C
/* single digit addition */
mp_err mp_add_d(const mp_int *a, mp_digit b, mp_int *c)
{
   mp_err     err;
   int ix, oldused;
   mp_digit *tmpa, *tmpc;

   /* grow c as required */
   if (c->alloc < (a->used + 1)) {
      if ((err = mp_grow(c, a->used + 1)) != MP_OKAY) {
         return err;
      }
   }

   /* if a is negative and |a| >= b, call c = |a| - b */
   if ((a->sign == MP_NEG) && ((a->used > 1) || (a->dp[0] >= b))) {
      mp_int a_ = *a;
      /* temporarily fix sign of a */
      a_.sign = MP_ZPOS;

      /* c = |a| - b */
      err = mp_sub_d(&a_, b, c);

      /* fix sign  */
      c->sign = MP_NEG;

      /* clamp */
      mp_clamp(c);

      return err;
   }

   /* old number of used digits in c */
   oldused = c->used;

   /* source alias */
   tmpa    = a->dp;

   /* destination alias */
   tmpc    = c->dp;

   /* if a is positive */
   if (a->sign == MP_ZPOS) {
      /* add digits, mu is carry */
      mp_digit mu = b;
      for (ix = 0; ix < a->used; ix++) {
         *tmpc   = *tmpa++ + mu;
         mu      = *tmpc >> MP_DIGIT_BIT;
         *tmpc++ &= MP_MASK;
      }
      /* set final carry */
      ix++;
      *tmpc++  = mu;

      /* setup size */
      c->used = a->used + 1;
   } else {
      /* a was negative and |a| < b */
      c->used  = 1;

      /* the result is a single digit */
      if (a->used == 1) {
         *tmpc++  =  b - a->dp[0];
      } else {
         *tmpc++  =  b;
      }

      /* setup count so the clearing of oldused
       * can fall through correctly
       */
      ix       = 1;
   }

   /* sign always positive */
   c->sign = MP_ZPOS;

   /* now zero to oldused */
   MP_ZERO_DIGITS(tmpc, oldused - ix);
   mp_clamp(c);

   return MP_OKAY;
}

#endif
#ifdef MP_AND_C
/* two complement and */
mp_err mp_and(const mp_int *a, const mp_int *b, mp_int *c)
{
   int used = MP_MAX(a->used, b->used) + 1, i;
   mp_err err;
   mp_digit ac = 1, bc = 1, cc = 1;
   mp_sign csign = ((a->sign == MP_NEG) && (b->sign == MP_NEG)) ? MP_NEG : MP_ZPOS;

   if (c->alloc < used) {
      if ((err = mp_grow(c, used)) != MP_OKAY) {
         return err;
      }
   }

   for (i = 0; i < used; i++) {
      mp_digit x, y;

      /* convert to two complement if negative */
      if (a->sign == MP_NEG) {
         ac += (i >= a->used) ? MP_MASK : (~a->dp[i] & MP_MASK);
         x = ac & MP_MASK;
         ac >>= MP_DIGIT_BIT;
      } else {
         x = (i >= a->used) ? 0uL : a->dp[i];
      }

      /* convert to two complement if negative */
      if (b->sign == MP_NEG) {
         bc += (i >= b->used) ? MP_MASK : (~b->dp[i] & MP_MASK);
         y = bc & MP_MASK;
         bc >>= MP_DIGIT_BIT;
      } else {
         y = (i >= b->used) ? 0uL : b->dp[i];
      }

      c->dp[i] = x & y;

      /* convert to to sign-magnitude if negative */
      if (csign == MP_NEG) {
         cc += ~c->dp[i] & MP_MASK;
         c->dp[i] = cc & MP_MASK;
         cc >>= MP_DIGIT_BIT;
      }
   }

   c->used = used;
   c->sign = csign;
   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef MP_CLAMP_C
/* trim unused digits
 *
 * This is used to ensure that leading zero digits are
 * trimed and the leading "used" digit will be non-zero
 * Typically very fast.  Also fixes the sign if there
 * are no more leading digits
 */
void mp_clamp(mp_int *a)
{
   /* decrease used while the most significant digit is
    * zero.
    */
   while ((a->used > 0) && (a->dp[a->used - 1] == 0u)) {
      --(a->used);
   }

   /* reset the sign flag if used == 0 */
   if (a->used == 0) {
      a->sign = MP_ZPOS;
   }
}
#endif
#ifdef MP_CLEAR_C
/* clear one (frees)  */
void mp_clear(mp_int *a)
{
   /* only do anything if a hasn't been freed previously */
   if (a->dp != NULL) {
      /* free ram */
      MP_FREE_DIGITS(a->dp, a->alloc);

      /* reset members to make debugging easier */
      a->dp    = NULL;
      a->alloc = a->used = 0;
      a->sign  = MP_ZPOS;
   }
}
#endif
#ifdef MP_CMP_C
/* compare two ints (signed)*/
mp_ord mp_cmp(const mp_int *a, const mp_int *b)
{
   /* compare based on sign */
   if (a->sign != b->sign) {
      if (a->sign == MP_NEG) {
         return MP_LT;
      } else {
         return MP_GT;
      }
   }

   /* compare digits */
   if (a->sign == MP_NEG) {
      /* if negative compare opposite direction */
      return mp_cmp_mag(b, a);
   } else {
      return mp_cmp_mag(a, b);
   }
}
#endif
#ifdef MP_CMP_MAG_C
/* compare maginitude of two ints (unsigned) */
mp_ord mp_cmp_mag(const mp_int *a, const mp_int *b)
{
   int     n;
   const mp_digit *tmpa, *tmpb;

   /* compare based on # of non-zero digits */
   if (a->used > b->used) {
      return MP_GT;
   }

   if (a->used < b->used) {
      return MP_LT;
   }

   /* alias for a */
   tmpa = a->dp + (a->used - 1);

   /* alias for b */
   tmpb = b->dp + (a->used - 1);

   /* compare based on digits  */
   for (n = 0; n < a->used; ++n, --tmpa, --tmpb) {
      if (*tmpa > *tmpb) {
         return MP_GT;
      }

      if (*tmpa < *tmpb) {
         return MP_LT;
      }
   }
   return MP_EQ;
}
#endif
#ifdef MP_COMPLEMENT_C
/* b = ~a */
mp_err mp_complement(const mp_int *a, mp_int *b)
{
   mp_err err = mp_neg(a, b);
   return (err == MP_OKAY) ? mp_sub_d(b, 1uL, b) : err;
}
#endif
#ifdef MP_COPY_C
/* copy, b = a */
mp_err mp_copy(const mp_int *a, mp_int *b)
{
   int n;
   mp_err err;

   /* if dst == src do nothing */
   if (a == b) {
      return MP_OKAY;
   }

   /* grow dest */
   if (b->alloc < a->used) {
      if ((err = mp_grow(b, a->used)) != MP_OKAY) {
         return err;
      }
   }

   /* zero b and copy the parameters over */
   {
      mp_digit *tmpa, *tmpb;

      /* pointer aliases */

      /* source */
      tmpa = a->dp;

      /* destination */
      tmpb = b->dp;

      /* copy all the digits */
      for (n = 0; n < a->used; n++) {
         *tmpb++ = *tmpa++;
      }

      /* clear high digits */
      MP_ZERO_DIGITS(tmpb, b->used - n);
   }

   /* copy used count and sign */
   b->used = a->used;
   b->sign = a->sign;
   return MP_OKAY;
}
#endif
#ifdef MP_COUNT_BITS_C
/* returns the number of bits in an int */
int mp_count_bits(const mp_int *a)
{
   int     r;
   mp_digit q;

   /* shortcut */
   if (MP_IS_ZERO(a)) {
      return 0;
   }

   /* get number of digits and add that */
   r = (a->used - 1) * MP_DIGIT_BIT;

   /* take the last digit and count the bits in it */
   q = a->dp[a->used - 1];
   while (q > 0u) {
      ++r;
      q >>= 1u;
   }
   return r;
}
#endif
#ifdef MP_DIV_C
#ifdef MP_DIV_SMALL

/* slower bit-bang division... also smaller */
mp_err mp_div(const mp_int *a, const mp_int *b, mp_int *c, mp_int *d)
{
   mp_int ta, tb, tq, q;
   int     n, n2;
   mp_err err;

   /* is divisor zero ? */
   if (MP_IS_ZERO(b)) {
      return MP_VAL;
   }

   /* if a < b then q=0, r = a */
   if (mp_cmp_mag(a, b) == MP_LT) {
      if (d != NULL) {
         err = mp_copy(a, d);
      } else {
         err = MP_OKAY;
      }
      if (c != NULL) {
         mp_zero(c);
      }
      return err;
   }

   /* init our temps */
   if ((err = mp_init_multi(&ta, &tb, &tq, &q, NULL)) != MP_OKAY) {
      return err;
   }

   mp_set(&tq, 1uL);
   n = mp_count_bits(a) - mp_count_bits(b);
   if (((err = mp_abs(a, &ta)) != MP_OKAY) ||
       ((err = mp_abs(b, &tb)) != MP_OKAY) ||
       ((err = mp_mul_2d(&tb, n, &tb)) != MP_OKAY) ||
       ((err = mp_mul_2d(&tq, n, &tq)) != MP_OKAY)) {
      goto LBL_ERR;
   }

   while (n-- >= 0) {
      if (mp_cmp(&tb, &ta) != MP_GT) {
         if (((err = mp_sub(&ta, &tb, &ta)) != MP_OKAY) ||
             ((err = mp_add(&q, &tq, &q)) != MP_OKAY)) {
            goto LBL_ERR;
         }
      }
      if (((err = mp_div_2d(&tb, 1, &tb, NULL)) != MP_OKAY) ||
          ((err = mp_div_2d(&tq, 1, &tq, NULL)) != MP_OKAY)) {
         goto LBL_ERR;
      }
   }

   /* now q == quotient and ta == remainder */
   n  = a->sign;
   n2 = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;
   if (c != NULL) {
      mp_exch(c, &q);
      c->sign  = MP_IS_ZERO(c) ? MP_ZPOS : n2;
   }
   if (d != NULL) {
      mp_exch(d, &ta);
      d->sign = MP_IS_ZERO(d) ? MP_ZPOS : n;
   }
LBL_ERR:
   mp_clear_multi(&ta, &tb, &tq, &q, NULL);
   return err;
}

#else

/* integer signed division.
 * c*b + d == a [e.g. a/b, c=quotient, d=remainder]
 * HAC pp.598 Algorithm 14.20
 *
 * Note that the description in HAC is horribly
 * incomplete.  For example, it doesn't consider
 * the case where digits are removed from 'x' in
 * the inner loop.  It also doesn't consider the
 * case that y has fewer than three digits, etc..
 *
 * The overall algorithm is as described as
 * 14.20 from HAC but fixed to treat these cases.
*/
mp_err mp_div(const mp_int *a, const mp_int *b, mp_int *c, mp_int *d)
{
   mp_int  q, x, y, t1, t2;
   int     n, t, i, norm;
   mp_sign neg;
   mp_err  err;

   /* is divisor zero ? */
   if (MP_IS_ZERO(b)) {
      return MP_VAL;
   }

   /* if a < b then q=0, r = a */
   if (mp_cmp_mag(a, b) == MP_LT) {
      if (d != NULL) {
         err = mp_copy(a, d);
      } else {
         err = MP_OKAY;
      }
      if (c != NULL) {
         mp_zero(c);
      }
      return err;
   }

   if ((err = mp_init_size(&q, a->used + 2)) != MP_OKAY) {
      return err;
   }
   q.used = a->used + 2;

   if ((err = mp_init(&t1)) != MP_OKAY) {
      goto LBL_Q;
   }

   if ((err = mp_init(&t2)) != MP_OKAY) {
      goto LBL_T1;
   }

   if ((err = mp_init_copy(&x, a)) != MP_OKAY) {
      goto LBL_T2;
   }

   if ((err = mp_init_copy(&y, b)) != MP_OKAY) {
      goto LBL_X;
   }

   /* fix the sign */
   neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;
   x.sign = y.sign = MP_ZPOS;

   /* normalize both x and y, ensure that y >= b/2, [b == 2**MP_DIGIT_BIT] */
   norm = mp_count_bits(&y) % MP_DIGIT_BIT;
   if (norm < (MP_DIGIT_BIT - 1)) {
      norm = (MP_DIGIT_BIT - 1) - norm;
      if ((err = mp_mul_2d(&x, norm, &x)) != MP_OKAY) {
         goto LBL_Y;
      }
      if ((err = mp_mul_2d(&y, norm, &y)) != MP_OKAY) {
         goto LBL_Y;
      }
   } else {
      norm = 0;
   }

   /* note hac does 0 based, so if used==5 then its 0,1,2,3,4, e.g. use 4 */
   n = x.used - 1;
   t = y.used - 1;

   /* while (x >= y*b**n-t) do { q[n-t] += 1; x -= y*b**{n-t} } */
   if ((err = mp_lshd(&y, n - t)) != MP_OKAY) { /* y = y*b**{n-t} */
      goto LBL_Y;
   }

   while (mp_cmp(&x, &y) != MP_LT) {
      ++(q.dp[n - t]);
      if ((err = mp_sub(&x, &y, &x)) != MP_OKAY) {
         goto LBL_Y;
      }
   }

   /* reset y by shifting it back down */
   mp_rshd(&y, n - t);

   /* step 3. for i from n down to (t + 1) */
   for (i = n; i >= (t + 1); i--) {
      if (i > x.used) {
         continue;
      }

      /* step 3.1 if xi == yt then set q{i-t-1} to b-1,
       * otherwise set q{i-t-1} to (xi*b + x{i-1})/yt */
      if (x.dp[i] == y.dp[t]) {
         q.dp[(i - t) - 1] = ((mp_digit)1 << (mp_digit)MP_DIGIT_BIT) - (mp_digit)1;
      } else {
         mp_word tmp;
         tmp = (mp_word)x.dp[i] << (mp_word)MP_DIGIT_BIT;
         tmp |= (mp_word)x.dp[i - 1];
         tmp /= (mp_word)y.dp[t];
         if (tmp > (mp_word)MP_MASK) {
            tmp = MP_MASK;
         }
         q.dp[(i - t) - 1] = (mp_digit)(tmp & (mp_word)MP_MASK);
      }

      /* while (q{i-t-1} * (yt * b + y{t-1})) >
               xi * b**2 + xi-1 * b + xi-2

         do q{i-t-1} -= 1;
      */
      q.dp[(i - t) - 1] = (q.dp[(i - t) - 1] + 1uL) & (mp_digit)MP_MASK;
      do {
         q.dp[(i - t) - 1] = (q.dp[(i - t) - 1] - 1uL) & (mp_digit)MP_MASK;

         /* find left hand */
         mp_zero(&t1);
         t1.dp[0] = ((t - 1) < 0) ? 0u : y.dp[t - 1];
         t1.dp[1] = y.dp[t];
         t1.used = 2;
         if ((err = mp_mul_d(&t1, q.dp[(i - t) - 1], &t1)) != MP_OKAY) {
            goto LBL_Y;
         }

         /* find right hand */
         t2.dp[0] = ((i - 2) < 0) ? 0u : x.dp[i - 2];
         t2.dp[1] = x.dp[i - 1]; /* i >= 1 always holds */
         t2.dp[2] = x.dp[i];
         t2.used = 3;
      } while (mp_cmp_mag(&t1, &t2) == MP_GT);

      /* step 3.3 x = x - q{i-t-1} * y * b**{i-t-1} */
      if ((err = mp_mul_d(&y, q.dp[(i - t) - 1], &t1)) != MP_OKAY) {
         goto LBL_Y;
      }

      if ((err = mp_lshd(&t1, (i - t) - 1)) != MP_OKAY) {
         goto LBL_Y;
      }

      if ((err = mp_sub(&x, &t1, &x)) != MP_OKAY) {
         goto LBL_Y;
      }

      /* if x < 0 then { x = x + y*b**{i-t-1}; q{i-t-1} -= 1; } */
      if (x.sign == MP_NEG) {
         if ((err = mp_copy(&y, &t1)) != MP_OKAY) {
            goto LBL_Y;
         }
         if ((err = mp_lshd(&t1, (i - t) - 1)) != MP_OKAY) {
            goto LBL_Y;
         }
         if ((err = mp_add(&x, &t1, &x)) != MP_OKAY) {
            goto LBL_Y;
         }

         q.dp[(i - t) - 1] = (q.dp[(i - t) - 1] - 1uL) & MP_MASK;
      }
   }

   /* now q is the quotient and x is the remainder
    * [which we have to normalize]
    */

   /* get sign before writing to c */
   x.sign = (x.used == 0) ? MP_ZPOS : a->sign;

   if (c != NULL) {
      mp_clamp(&q);
      mp_exch(&q, c);
      c->sign = neg;
   }

   if (d != NULL) {
      if ((err = mp_div_2d(&x, norm, &x, NULL)) != MP_OKAY) {
         goto LBL_Y;
      }
      mp_exch(&x, d);
   }

   err = MP_OKAY;

LBL_Y:
   mp_clear(&y);
LBL_X:
   mp_clear(&x);
LBL_T2:
   mp_clear(&t2);
LBL_T1:
   mp_clear(&t1);
LBL_Q:
   mp_clear(&q);
   return err;
}

#endif

#endif
#ifdef MP_DIV_2D_C
/* shift right by a certain bit count (store quotient in c, optional remainder in d) */
mp_err mp_div_2d(const mp_int *a, int b, mp_int *c, mp_int *d)
{
   mp_digit D, r, rr;
   int     x;
   mp_err err;

   /* if the shift count is <= 0 then we do no work */
   if (b <= 0) {
      err = mp_copy(a, c);
      if (d != NULL) {
         mp_zero(d);
      }
      return err;
   }

   /* copy */
   if ((err = mp_copy(a, c)) != MP_OKAY) {
      return err;
   }
   /* 'a' should not be used after here - it might be the same as d */

   /* get the remainder */
   if (d != NULL) {
      if ((err = mp_mod_2d(a, b, d)) != MP_OKAY) {
         return err;
      }
   }

   /* shift by as many digits in the bit count */
   if (b >= MP_DIGIT_BIT) {
      mp_rshd(c, b / MP_DIGIT_BIT);
   }

   /* shift any bit count < MP_DIGIT_BIT */
   D = (mp_digit)(b % MP_DIGIT_BIT);
   if (D != 0u) {
      mp_digit *tmpc, mask, shift;

      /* mask */
      mask = ((mp_digit)1 << D) - 1uL;

      /* shift for lsb */
      shift = (mp_digit)MP_DIGIT_BIT - D;

      /* alias */
      tmpc = c->dp + (c->used - 1);

      /* carry */
      r = 0;
      for (x = c->used - 1; x >= 0; x--) {
         /* get the lower  bits of this word in a temp */
         rr = *tmpc & mask;

         /* shift the current word and mix in the carry bits from the previous word */
         *tmpc = (*tmpc >> D) | (r << shift);
         --tmpc;

         /* set the carry to the carry bits of the current word found above */
         r = rr;
      }
   }
   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef MP_DIV_D_C
/* single digit division (based on routine from MPI) */
mp_err mp_div_d(const mp_int *a, mp_digit b, mp_int *c, mp_digit *d)
{
   mp_int  q;
   mp_word w;
   mp_digit t;
   mp_err err;
   int ix;

   /* cannot divide by zero */
   if (b == 0u) {
      return MP_VAL;
   }

   /* quick outs */
   if ((b == 1u) || MP_IS_ZERO(a)) {
      if (d != NULL) {
         *d = 0;
      }
      if (c != NULL) {
         return mp_copy(a, c);
      }
      return MP_OKAY;
   }

   /* power of two ? */
   if ((b & (b-1)) == 0u) {
      ix = 1;
      while ((ix < MP_DIGIT_BIT) && (b != (((mp_digit)1)<<ix))) {
         ix++;
      }
      if (d != NULL) {
         *d = a->dp[0] & (((mp_digit)1<<(mp_digit)ix) - 1uL);
      }
      if (c != NULL) {
         return mp_div_2d(a, ix, c, NULL);
      }
      return MP_OKAY;
   }

   /* three? */
   if (MP_HAS(MP_DIV_3) && b == 3u) {
      return mp_div_3(a, c, d);
   }

   /* no easy answer [c'est la vie].  Just division */
   if ((err = mp_init_size(&q, a->used)) != MP_OKAY) {
      return err;
   }

   q.used = a->used;
   q.sign = a->sign;
   w = 0;
   for (ix = a->used - 1; ix >= 0; ix--) {
      w = (w << (mp_word)MP_DIGIT_BIT) | (mp_word)a->dp[ix];

      if (w >= b) {
         t = (mp_digit)(w / b);
         w -= (mp_word)t * (mp_word)b;
      } else {
         t = 0;
      }
      q.dp[ix] = t;
   }

   if (d != NULL) {
      *d = (mp_digit)w;
   }

   if (c != NULL) {
      mp_clamp(&q);
      mp_exch(&q, c);
   }
   mp_clear(&q);

   return err;
}

#endif
#ifdef MP_EXCH_C
/* swap the elements of two integers, for cases where you can't simply swap the
 * mp_int pointers around
 */
void mp_exch(mp_int *a, mp_int *b)
{
   mp_int  t;

   t  = *a;
   *a = *b;
   *b = t;
}
#endif
#ifdef MP_GET_DOUBLE_C
double mp_get_double(const mp_int *a)
{
   int i;
   double d = 0.0, fac = 1.0;
   for (i = 0; i < MP_DIGIT_BIT; ++i) {
      fac *= 2.0;
   }
   for (i = a->used; i --> 0;) {
      d = (d * fac) + (double)a->dp[i];
   }
   return (a->sign == MP_NEG) ? -d : d;
}
#endif
#ifdef MP_GET_I64_C
MP_GET_SIGNED(int64_t, mp_get_i64, mp_get_mag64)
#endif
#ifdef MP_GET_MAG64_C
MP_GET_MAG(uint64_t, mp_get_mag64)
#endif
#ifdef MP_GROW_C
/* grow as required */
mp_err mp_grow(mp_int *a, int size)
{
   int     i;
   mp_digit *tmp;

   /* if the alloc size is smaller alloc more ram */
   if (a->alloc < size) {
      /* reallocate the array a->dp
       *
       * We store the return in a temporary variable
       * in case the operation failed we don't want
       * to overwrite the dp member of a.
       */
      tmp = (mp_digit *) MP_REALLOC(a->dp,
                                    (size_t)a->alloc * sizeof(mp_digit),
                                    (size_t)size * sizeof(mp_digit));
      if (tmp == NULL) {
         /* reallocation failed but "a" is still valid [can be freed] */
         return MP_MEM;
      }

      /* reallocation succeeded so set a->dp */
      a->dp = tmp;

      /* zero excess digits */
      i        = a->alloc;
      a->alloc = size;
      MP_ZERO_DIGITS(a->dp + i, a->alloc - i);
   }
   return MP_OKAY;
}
#endif
#ifdef MP_INIT_C
/* init a new mp_int */
mp_err mp_init(mp_int *a)
{
   /* allocate memory required and clear it */
   a->dp = (mp_digit *) MP_CALLOC((size_t)MP_PREC, sizeof(mp_digit));
   if (a->dp == NULL) {
      return MP_MEM;
   }

   /* set the used to zero, allocated digits to the default precision
    * and sign to positive */
   a->used  = 0;
   a->alloc = MP_PREC;
   a->sign  = MP_ZPOS;

   return MP_OKAY;
}
#endif
#ifdef MP_INIT_COPY_C
/* creates "a" then copies b into it */
mp_err mp_init_copy(mp_int *a, const mp_int *b)
{
   mp_err     err;

   if ((err = mp_init_size(a, b->used)) != MP_OKAY) {
      return err;
   }

   if ((err = mp_copy(b, a)) != MP_OKAY) {
      mp_clear(a);
   }

   return err;
}
#endif
#ifdef MP_INIT_SIZE_C
/* init an mp_init for a given size */
mp_err mp_init_size(mp_int *a, int size)
{
   size = MP_MAX(MP_MIN_PREC, size);

   /* alloc mem */
   a->dp = (mp_digit *) MP_CALLOC((size_t)size, sizeof(mp_digit));
   if (a->dp == NULL) {
      return MP_MEM;
   }

   /* set the members */
   a->used  = 0;
   a->alloc = size;
   a->sign  = MP_ZPOS;

   return MP_OKAY;
}
#endif
#ifdef MP_LSHD_C
/* shift left a certain amount of digits */
mp_err mp_lshd(mp_int *a, int b)
{
   int x;
   mp_err err;
   mp_digit *top, *bottom;

   /* if its less than zero return */
   if (b <= 0) {
      return MP_OKAY;
   }
   /* no need to shift 0 around */
   if (MP_IS_ZERO(a)) {
      return MP_OKAY;
   }

   /* grow to fit the new digits */
   if (a->alloc < (a->used + b)) {
      if ((err = mp_grow(a, a->used + b)) != MP_OKAY) {
         return err;
      }
   }

   /* increment the used by the shift amount then copy upwards */
   a->used += b;

   /* top */
   top = a->dp + a->used - 1;

   /* base */
   bottom = (a->dp + a->used - 1) - b;

   /* much like mp_rshd this is implemented using a sliding window
    * except the window goes the otherway around.  Copying from
    * the bottom to the top.  see mp_rshd.c for more info.
    */
   for (x = a->used - 1; x >= b; x--) {
      *top-- = *bottom--;
   }

   /* zero the lower digits */
   MP_ZERO_DIGITS(a->dp, b);

   return MP_OKAY;
}
#endif
#ifdef MP_MOD_C
/* c = a mod b, 0 <= c < b if b > 0, b < c <= 0 if b < 0 */
mp_err mp_mod(const mp_int *a, const mp_int *b, mp_int *c)
{
   mp_int  t;
   mp_err  err;

   if ((err = mp_init_size(&t, b->used)) != MP_OKAY) {
      return err;
   }

   if ((err = mp_div(a, b, NULL, &t)) != MP_OKAY) {
      mp_clear(&t);
      return err;
   }

   if (MP_IS_ZERO(&t) || (t.sign == b->sign)) {
      err = MP_OKAY;
      mp_exch(&t, c);
   } else {
      err = mp_add(b, &t, c);
   }

   mp_clear(&t);
   return err;
}
#endif
#ifdef MP_MOD_2D_C
/* calc a value mod 2**b */
mp_err mp_mod_2d(const mp_int *a, int b, mp_int *c)
{
   int x;
   mp_err err;

   /* if b is <= 0 then zero the int */
   if (b <= 0) {
      mp_zero(c);
      return MP_OKAY;
   }

   /* if the modulus is larger than the value than return */
   if (b >= (a->used * MP_DIGIT_BIT)) {
      return mp_copy(a, c);
   }

   /* copy */
   if ((err = mp_copy(a, c)) != MP_OKAY) {
      return err;
   }

   /* zero digits above the last digit of the modulus */
   x = (b / MP_DIGIT_BIT) + (((b % MP_DIGIT_BIT) == 0) ? 0 : 1);
   MP_ZERO_DIGITS(c->dp + x, c->used - x);

   /* clear the digit that is not completely outside/inside the modulus */
   c->dp[b / MP_DIGIT_BIT] &=
      ((mp_digit)1 << (mp_digit)(b % MP_DIGIT_BIT)) - (mp_digit)1;
   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef MP_MUL_C
/* high level multiplication (handles sign) */
mp_err mp_mul(const mp_int *a, const mp_int *b, mp_int *c)
{
   mp_err err;
   int min_len = MP_MIN(a->used, b->used),
       max_len = MP_MAX(a->used, b->used),
       digs = a->used + b->used + 1;
   mp_sign neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;

   if (MP_HAS(S_MP_BALANCE_MUL) &&
       /* Check sizes. The smaller one needs to be larger than the Karatsuba cut-off.
        * The bigger one needs to be at least about one MP_KARATSUBA_MUL_CUTOFF bigger
        * to make some sense, but it depends on architecture, OS, position of the
        * stars... so YMMV.
        * Using it to cut the input into slices small enough for fast_s_mp_mul_digs
        * was actually slower on the author's machine, but YMMV.
        */
       (min_len >= MP_KARATSUBA_MUL_CUTOFF) &&
       (max_len / 2 >= MP_KARATSUBA_MUL_CUTOFF) &&
       /* Not much effect was observed below a ratio of 1:2, but again: YMMV. */
       (max_len >= (2 * min_len))) {
      err = s_mp_balance_mul(a,b,c);
   } else if (MP_HAS(S_MP_TOOM_MUL) &&
              (min_len >= MP_TOOM_MUL_CUTOFF)) {
      err = s_mp_toom_mul(a, b, c);
   } else if (MP_HAS(S_MP_KARATSUBA_MUL) &&
              (min_len >= MP_KARATSUBA_MUL_CUTOFF)) {
      err = s_mp_karatsuba_mul(a, b, c);
   } else if (MP_HAS(S_MP_MUL_DIGS_FAST) &&
              /* can we use the fast multiplier?
               *
               * The fast multiplier can be used if the output will
               * have less than MP_WARRAY digits and the number of
               * digits won't affect carry propagation
               */
              (digs < MP_WARRAY) &&
              (min_len <= MP_MAXFAST)) {
      err = s_mp_mul_digs_fast(a, b, c, digs);
   } else if (MP_HAS(S_MP_MUL_DIGS)) {
      err = s_mp_mul_digs(a, b, c, digs);
   } else {
      err = MP_VAL;
   }
   c->sign = (c->used > 0) ? neg : MP_ZPOS;
   return err;
}
#endif
#ifdef MP_MUL_2D_C
/* shift left by a certain bit count */
mp_err mp_mul_2d(const mp_int *a, int b, mp_int *c)
{
   mp_digit d;
   mp_err   err;

   /* copy */
   if (a != c) {
      if ((err = mp_copy(a, c)) != MP_OKAY) {
         return err;
      }
   }

   if (c->alloc < (c->used + (b / MP_DIGIT_BIT) + 1)) {
      if ((err = mp_grow(c, c->used + (b / MP_DIGIT_BIT) + 1)) != MP_OKAY) {
         return err;
      }
   }

   /* shift by as many digits in the bit count */
   if (b >= MP_DIGIT_BIT) {
      if ((err = mp_lshd(c, b / MP_DIGIT_BIT)) != MP_OKAY) {
         return err;
      }
   }

   /* shift any bit count < MP_DIGIT_BIT */
   d = (mp_digit)(b % MP_DIGIT_BIT);
   if (d != 0u) {
      mp_digit *tmpc, shift, mask, r, rr;
      int x;

      /* bitmask for carries */
      mask = ((mp_digit)1 << d) - (mp_digit)1;

      /* shift for msbs */
      shift = (mp_digit)MP_DIGIT_BIT - d;

      /* alias */
      tmpc = c->dp;

      /* carry */
      r    = 0;
      for (x = 0; x < c->used; x++) {
         /* get the higher bits of the current word */
         rr = (*tmpc >> shift) & mask;

         /* shift the current word and OR in the carry */
         *tmpc = ((*tmpc << d) | r) & MP_MASK;
         ++tmpc;

         /* set the carry to the carry bits of the current word */
         r = rr;
      }

      /* set final carry */
      if (r != 0u) {
         c->dp[(c->used)++] = r;
      }
   }
   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef MP_MUL_D_C
/* multiply by a digit */
mp_err mp_mul_d(const mp_int *a, mp_digit b, mp_int *c)
{
   mp_digit u, *tmpa, *tmpc;
   mp_word  r;
   mp_err   err;
   int      ix, olduse;

   /* make sure c is big enough to hold a*b */
   if (c->alloc < (a->used + 1)) {
      if ((err = mp_grow(c, a->used + 1)) != MP_OKAY) {
         return err;
      }
   }

   /* get the original destinations used count */
   olduse = c->used;

   /* set the sign */
   c->sign = a->sign;

   /* alias for a->dp [source] */
   tmpa = a->dp;

   /* alias for c->dp [dest] */
   tmpc = c->dp;

   /* zero carry */
   u = 0;

   /* compute columns */
   for (ix = 0; ix < a->used; ix++) {
      /* compute product and carry sum for this term */
      r       = (mp_word)u + ((mp_word)*tmpa++ * (mp_word)b);

      /* mask off higher bits to get a single digit */
      *tmpc++ = (mp_digit)(r & (mp_word)MP_MASK);

      /* send carry into next iteration */
      u       = (mp_digit)(r >> (mp_word)MP_DIGIT_BIT);
   }

   /* store final carry [if any] and increment ix offset  */
   *tmpc++ = u;
   ++ix;

   /* now zero digits above the top */
   MP_ZERO_DIGITS(tmpc, olduse - ix);

   /* set used count */
   c->used = a->used + 1;
   mp_clamp(c);

   return MP_OKAY;
}
#endif
#ifdef MP_NEG_C
/* b = -a */
mp_err mp_neg(const mp_int *a, mp_int *b)
{
   mp_err err;
   if (a != b) {
      if ((err = mp_copy(a, b)) != MP_OKAY) {
         return err;
      }
   }

   if (!MP_IS_ZERO(b)) {
      b->sign = (a->sign == MP_ZPOS) ? MP_NEG : MP_ZPOS;
   } else {
      b->sign = MP_ZPOS;
   }

   return MP_OKAY;
}
#endif
#ifdef MP_OR_C
/* two complement or */
mp_err mp_or(const mp_int *a, const mp_int *b, mp_int *c)
{
   int used = MP_MAX(a->used, b->used) + 1, i;
   mp_err err;
   mp_digit ac = 1, bc = 1, cc = 1;
   mp_sign csign = ((a->sign == MP_NEG) || (b->sign == MP_NEG)) ? MP_NEG : MP_ZPOS;

   if (c->alloc < used) {
      if ((err = mp_grow(c, used)) != MP_OKAY) {
         return err;
      }
   }

   for (i = 0; i < used; i++) {
      mp_digit x, y;

      /* convert to two complement if negative */
      if (a->sign == MP_NEG) {
         ac += (i >= a->used) ? MP_MASK : (~a->dp[i] & MP_MASK);
         x = ac & MP_MASK;
         ac >>= MP_DIGIT_BIT;
      } else {
         x = (i >= a->used) ? 0uL : a->dp[i];
      }

      /* convert to two complement if negative */
      if (b->sign == MP_NEG) {
         bc += (i >= b->used) ? MP_MASK : (~b->dp[i] & MP_MASK);
         y = bc & MP_MASK;
         bc >>= MP_DIGIT_BIT;
      } else {
         y = (i >= b->used) ? 0uL : b->dp[i];
      }

      c->dp[i] = x | y;

      /* convert to to sign-magnitude if negative */
      if (csign == MP_NEG) {
         cc += ~c->dp[i] & MP_MASK;
         c->dp[i] = cc & MP_MASK;
         cc >>= MP_DIGIT_BIT;
      }
   }

   c->used = used;
   c->sign = csign;
   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef MP_RADIX_SIZE_C
/* returns size of ASCII reprensentation */
mp_err mp_radix_size(const mp_int *a, int radix, int *size)
{
   mp_err  err;
   int     digs;
   mp_int   t;
   mp_digit d;

   *size = 0;

   /* make sure the radix is in range */
   if ((radix < 2) || (radix > 64)) {
      return MP_VAL;
   }

   if (MP_IS_ZERO(a)) {
      *size = 2;
      return MP_OKAY;
   }

   /* special case for binary */
   if (radix == 2) {
      *size = mp_count_bits(a) + ((a->sign == MP_NEG) ? 1 : 0) + 1;
      return MP_OKAY;
   }

   /* digs is the digit count */
   digs = 0;

   /* if it's negative add one for the sign */
   if (a->sign == MP_NEG) {
      ++digs;
   }

   /* init a copy of the input */
   if ((err = mp_init_copy(&t, a)) != MP_OKAY) {
      return err;
   }

   /* force temp to positive */
   t.sign = MP_ZPOS;

   /* fetch out all of the digits */
   while (!MP_IS_ZERO(&t)) {
      if ((err = mp_div_d(&t, (mp_digit)radix, &t, &d)) != MP_OKAY) {
         mp_clear(&t);
         return err;
      }
      ++digs;
   }
   mp_clear(&t);

   /* return digs + 1, the 1 is for the NULL byte that would be required. */
   *size = digs + 1;
   return MP_OKAY;
}

#endif
#ifdef MP_RADIX_SMAP_C
/* chars used in radix conversions */
const char *const mp_s_rmap = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";
const uint8_t mp_s_rmap_reverse[] = {
   0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f, /* ()*+,-./ */
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 01234567 */
   0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 89:;<=>? */
   0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, /* @ABCDEFG */
   0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, /* HIJKLMNO */
   0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, /* PQRSTUVW */
   0x21, 0x22, 0x23, 0xff, 0xff, 0xff, 0xff, 0xff, /* XYZ[\]^_ */
   0xff, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, /* `abcdefg */
   0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, /* hijklmno */
   0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, /* pqrstuvw */
   0x3b, 0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, /* xyz{|}~. */
};
const size_t mp_s_rmap_reverse_sz = sizeof(mp_s_rmap_reverse);
#endif
#ifdef MP_READ_UNSIGNED_BIN_C
/* reads a unsigned char array, assumes the msb is stored first [big endian] */
mp_err mp_read_unsigned_bin(mp_int *a, const unsigned char *b, int c)
{
   mp_err err;

   /* make sure there are at least two digits */
   if (a->alloc < 2) {
      if ((err = mp_grow(a, 2)) != MP_OKAY) {
         return err;
      }
   }

   /* zero the int */
   mp_zero(a);

   /* read the bytes in */
   while (c-- > 0) {
      if ((err = mp_mul_2d(a, 8, a)) != MP_OKAY) {
         return err;
      }

#ifndef MP_8BIT
      a->dp[0] |= *b++;
      a->used += 1;
#else
      a->dp[0] = (*b & MP_MASK);
      a->dp[1] |= ((*b++ >> 7) & 1u);
      a->used += 2;
#endif
   }
   mp_clamp(a);
   return MP_OKAY;
}
#endif
#ifdef MP_RSHD_C
/* shift right a certain amount of digits */
void mp_rshd(mp_int *a, int b)
{
   int     x;
   mp_digit *bottom, *top;

   /* if b <= 0 then ignore it */
   if (b <= 0) {
      return;
   }

   /* if b > used then simply zero it and return */
   if (a->used <= b) {
      mp_zero(a);
      return;
   }

   /* shift the digits down */

   /* bottom */
   bottom = a->dp;

   /* top [offset into digits] */
   top = a->dp + b;

   /* this is implemented as a sliding window where
    * the window is b-digits long and digits from
    * the top of the window are copied to the bottom
    *
    * e.g.

    b-2 | b-1 | b0 | b1 | b2 | ... | bb |   ---->
                /\                   |      ---->
                 \-------------------/      ---->
    */
   for (x = 0; x < (a->used - b); x++) {
      *bottom++ = *top++;
   }

   /* zero the top digits */
   MP_ZERO_DIGITS(bottom, a->used - x);

   /* remove excess digits */
   a->used -= b;
}
#endif
#ifdef MP_SET_DOUBLE_C
#if 1
mp_err mp_set_double(mp_int *a, double b)
{
   uint64_t frac;
   int exp;
   mp_err err;
   union {
      double   dbl;
      uint64_t bits;
   } cast;
   cast.dbl = b;

   exp = (int)((unsigned)(cast.bits >> 52) & 0x7FFu);
   frac = (cast.bits & ((1uLL << 52) - 1uLL)) | (1uLL << 52);

   if (exp == 0x7FF) { /* +-inf, NaN */
      return MP_VAL;
   }
   exp -= 1023 + 52;

   mp_set_u64(a, frac);

   err = (exp < 0) ? mp_div_2d(a, -exp, a, NULL) : mp_mul_2d(a, exp, a);
   if (err != MP_OKAY) {
      return err;
   }

   if (((cast.bits >> 63) != 0uLL) && !MP_IS_ZERO(a)) {
      a->sign = MP_NEG;
   }

   return MP_OKAY;
}
#else
/* pragma message() not supported by several compilers (in mostly older but still used versions) */
#  ifdef _MSC_VER
#    pragma message("mp_set_double implementation is only available on platforms with IEEE754 floating point format")
#  else
#    warning "mp_set_double implementation is only available on platforms with IEEE754 floating point format"
#  endif
#endif
#endif
#ifdef MP_SET_I64_C
MP_SET_SIGNED(mp_set_i64, mp_set_u64, int64_t, uint64_t)
#endif
#ifdef MP_SET_U64_C
MP_SET_UNSIGNED(mp_set_u64, uint64_t)
#endif
#ifdef MP_SIGNED_RSH_C
/* shift right by a certain bit count with sign extension */
mp_err mp_signed_rsh(const mp_int *a, int b, mp_int *c)
{
   mp_err res;
   if (a->sign == MP_ZPOS) {
      return mp_div_2d(a, b, c, NULL);
   }

   res = mp_add_d(a, 1uL, c);
   if (res != MP_OKAY) {
      return res;
   }

   res = mp_div_2d(c, b, c, NULL);
   return (res == MP_OKAY) ? mp_sub_d(c, 1uL, c) : res;
}
#endif
#ifdef MP_SUB_C
/* high level subtraction (handles signs) */
mp_err mp_sub(const mp_int *a, const mp_int *b, mp_int *c)
{
   mp_sign sa = a->sign, sb = b->sign;
   mp_err err;

   if (sa != sb) {
      /* subtract a negative from a positive, OR */
      /* subtract a positive from a negative. */
      /* In either case, ADD their magnitudes, */
      /* and use the sign of the first number. */
      c->sign = sa;
      err = s_mp_add(a, b, c);
   } else {
      /* subtract a positive from a positive, OR */
      /* subtract a negative from a negative. */
      /* First, take the difference between their */
      /* magnitudes, then... */
      if (mp_cmp_mag(a, b) != MP_LT) {
         /* Copy the sign from the first */
         c->sign = sa;
         /* The first has a larger or equal magnitude */
         err = s_mp_sub(a, b, c);
      } else {
         /* The result has the *opposite* sign from */
         /* the first number. */
         c->sign = (sa == MP_ZPOS) ? MP_NEG : MP_ZPOS;
         /* The second has a larger magnitude */
         err = s_mp_sub(b, a, c);
      }
   }
   return err;
}

#endif
#ifdef MP_SUB_D_C
/* single digit subtraction */
mp_err mp_sub_d(const mp_int *a, mp_digit b, mp_int *c)
{
   mp_digit *tmpa, *tmpc;
   mp_err    err;
   int       ix, oldused;

   /* grow c as required */
   if (c->alloc < (a->used + 1)) {
      if ((err = mp_grow(c, a->used + 1)) != MP_OKAY) {
         return err;
      }
   }

   /* if a is negative just do an unsigned
    * addition [with fudged signs]
    */
   if (a->sign == MP_NEG) {
      mp_int a_ = *a;
      a_.sign = MP_ZPOS;
      err     = mp_add_d(&a_, b, c);
      c->sign = MP_NEG;

      /* clamp */
      mp_clamp(c);

      return err;
   }

   /* setup regs */
   oldused = c->used;
   tmpa    = a->dp;
   tmpc    = c->dp;

   /* if a <= b simply fix the single digit */
   if (((a->used == 1) && (a->dp[0] <= b)) || (a->used == 0)) {
      if (a->used == 1) {
         *tmpc++ = b - *tmpa;
      } else {
         *tmpc++ = b;
      }
      ix      = 1;

      /* negative/1digit */
      c->sign = MP_NEG;
      c->used = 1;
   } else {
      mp_digit mu = b;

      /* positive/size */
      c->sign = MP_ZPOS;
      c->used = a->used;

      /* subtract digits, mu is carry */
      for (ix = 0; ix < a->used; ix++) {
         *tmpc    = *tmpa++ - mu;
         mu       = *tmpc >> (MP_SIZEOF_BITS(mp_digit) - 1u);
         *tmpc++ &= MP_MASK;
      }
   }

   /* zero excess digits */
   MP_ZERO_DIGITS(tmpc, oldused - ix);

   mp_clamp(c);
   return MP_OKAY;
}

#endif
#ifdef MP_TORADIX_C
/* stores a bignum as a ASCII string in a given radix (2..64) */
mp_err mp_toradix(const mp_int *a, char *str, int radix)
{
   mp_err  err;
   int digs;
   mp_int  t;
   mp_digit d;
   char   *_s = str;

   /* check range of the radix */
   if ((radix < 2) || (radix > 64)) {
      return MP_VAL;
   }

   /* quick out if its zero */
   if (MP_IS_ZERO(a)) {
      *str++ = '0';
      *str = '\0';
      return MP_OKAY;
   }

   if ((err = mp_init_copy(&t, a)) != MP_OKAY) {
      return err;
   }

   /* if it is negative output a - */
   if (t.sign == MP_NEG) {
      ++_s;
      *str++ = '-';
      t.sign = MP_ZPOS;
   }

   digs = 0;
   while (!MP_IS_ZERO(&t)) {
      if ((err = mp_div_d(&t, (mp_digit)radix, &t, &d)) != MP_OKAY) {
         mp_clear(&t);
         return err;
      }
      *str++ = mp_s_rmap[d];
      ++digs;
   }

   /* reverse the digits of the string.  In this case _s points
    * to the first digit [exluding the sign] of the number]
    */
   s_mp_reverse((unsigned char *)_s, digs);

   /* append a NULL so the string is properly terminated */
   *str = '\0';

   mp_clear(&t);
   return MP_OKAY;
}

#endif
#ifdef MP_XOR_C
/* two complement xor */
mp_err mp_xor(const mp_int *a, const mp_int *b, mp_int *c)
{
   int used = MP_MAX(a->used, b->used) + 1, i;
   mp_err err;
   mp_digit ac = 1, bc = 1, cc = 1;
   mp_sign csign = (a->sign != b->sign) ? MP_NEG : MP_ZPOS;

   if (c->alloc < used) {
      if ((err = mp_grow(c, used)) != MP_OKAY) {
         return err;
      }
   }

   for (i = 0; i < used; i++) {
      mp_digit x, y;

      /* convert to two complement if negative */
      if (a->sign == MP_NEG) {
         ac += (i >= a->used) ? MP_MASK : (~a->dp[i] & MP_MASK);
         x = ac & MP_MASK;
         ac >>= MP_DIGIT_BIT;
      } else {
         x = (i >= a->used) ? 0uL : a->dp[i];
      }

      /* convert to two complement if negative */
      if (b->sign == MP_NEG) {
         bc += (i >= b->used) ? MP_MASK : (~b->dp[i] & MP_MASK);
         y = bc & MP_MASK;
         bc >>= MP_DIGIT_BIT;
      } else {
         y = (i >= b->used) ? 0uL : b->dp[i];
      }

      c->dp[i] = x ^ y;

      /* convert to to sign-magnitude if negative */
      if (csign == MP_NEG) {
         cc += ~c->dp[i] & MP_MASK;
         c->dp[i] = cc & MP_MASK;
         cc >>= MP_DIGIT_BIT;
      }
   }

   c->used = used;
   c->sign = csign;
   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef MP_ZERO_C
/* set to zero */
void mp_zero(mp_int *a)
{
   a->sign = MP_ZPOS;
   a->used = 0;
   MP_ZERO_DIGITS(a->dp, a->alloc);
}
#endif
#ifdef S_MP_ADD_C
/* low level addition, based on HAC pp.594, Algorithm 14.7 */
mp_err s_mp_add(const mp_int *a, const mp_int *b, mp_int *c)
{
   const mp_int *x;
   mp_err err;
   int     olduse, min, max;

   /* find sizes, we let |a| <= |b| which means we have to sort
    * them.  "x" will point to the input with the most digits
    */
   if (a->used > b->used) {
      min = b->used;
      max = a->used;
      x = a;
   } else {
      min = a->used;
      max = b->used;
      x = b;
   }

   /* init result */
   if (c->alloc < (max + 1)) {
      if ((err = mp_grow(c, max + 1)) != MP_OKAY) {
         return err;
      }
   }

   /* get old used digit count and set new one */
   olduse = c->used;
   c->used = max + 1;

   {
      mp_digit u, *tmpa, *tmpb, *tmpc;
      int i;

      /* alias for digit pointers */

      /* first input */
      tmpa = a->dp;

      /* second input */
      tmpb = b->dp;

      /* destination */
      tmpc = c->dp;

      /* zero the carry */
      u = 0;
      for (i = 0; i < min; i++) {
         /* Compute the sum at one digit, T[i] = A[i] + B[i] + U */
         *tmpc = *tmpa++ + *tmpb++ + u;

         /* U = carry bit of T[i] */
         u = *tmpc >> (mp_digit)MP_DIGIT_BIT;

         /* take away carry bit from T[i] */
         *tmpc++ &= MP_MASK;
      }

      /* now copy higher words if any, that is in A+B
       * if A or B has more digits add those in
       */
      if (min != max) {
         for (; i < max; i++) {
            /* T[i] = X[i] + U */
            *tmpc = x->dp[i] + u;

            /* U = carry bit of T[i] */
            u = *tmpc >> (mp_digit)MP_DIGIT_BIT;

            /* take away carry bit from T[i] */
            *tmpc++ &= MP_MASK;
         }
      }

      /* add carry */
      *tmpc++ = u;

      /* clear digits above oldused */
      MP_ZERO_DIGITS(tmpc, olduse - c->used);
   }

   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef S_MP_KARATSUBA_MUL_C
/* c = |a| * |b| using Karatsuba Multiplication using
 * three half size multiplications
 *
 * Let B represent the radix [e.g. 2**MP_DIGIT_BIT] and
 * let n represent half of the number of digits in
 * the min(a,b)
 *
 * a = a1 * B**n + a0
 * b = b1 * B**n + b0
 *
 * Then, a * b =>
   a1b1 * B**2n + ((a1 + a0)(b1 + b0) - (a0b0 + a1b1)) * B + a0b0
 *
 * Note that a1b1 and a0b0 are used twice and only need to be
 * computed once.  So in total three half size (half # of
 * digit) multiplications are performed, a0b0, a1b1 and
 * (a1+b1)(a0+b0)
 *
 * Note that a multiplication of half the digits requires
 * 1/4th the number of single precision multiplications so in
 * total after one call 25% of the single precision multiplications
 * are saved.  Note also that the call to mp_mul can end up back
 * in this function if the a0, a1, b0, or b1 are above the threshold.
 * This is known as divide-and-conquer and leads to the famous
 * O(N**lg(3)) or O(N**1.584) work which is asymptopically lower than
 * the standard O(N**2) that the baseline/comba methods use.
 * Generally though the overhead of this method doesn't pay off
 * until a certain size (N ~ 80) is reached.
 */
mp_err s_mp_karatsuba_mul(const mp_int *a, const mp_int *b, mp_int *c)
{
   mp_int  x0, x1, y0, y1, t1, x0y0, x1y1;
   int     B;
   mp_err  err = MP_MEM; /* default the return code to an error */

   /* min # of digits */
   B = MP_MIN(a->used, b->used);

   /* now divide in two */
   B = B >> 1;

   /* init copy all the temps */
   if (mp_init_size(&x0, B) != MP_OKAY)
      goto LBL_ERR;
   if (mp_init_size(&x1, a->used - B) != MP_OKAY)
      goto X0;
   if (mp_init_size(&y0, B) != MP_OKAY)
      goto X1;
   if (mp_init_size(&y1, b->used - B) != MP_OKAY)
      goto Y0;

   /* init temps */
   if (mp_init_size(&t1, B * 2) != MP_OKAY)
      goto Y1;
   if (mp_init_size(&x0y0, B * 2) != MP_OKAY)
      goto T1;
   if (mp_init_size(&x1y1, B * 2) != MP_OKAY)
      goto X0Y0;

   /* now shift the digits */
   x0.used = y0.used = B;
   x1.used = a->used - B;
   y1.used = b->used - B;

   {
      int x;
      mp_digit *tmpa, *tmpb, *tmpx, *tmpy;

      /* we copy the digits directly instead of using higher level functions
       * since we also need to shift the digits
       */
      tmpa = a->dp;
      tmpb = b->dp;

      tmpx = x0.dp;
      tmpy = y0.dp;
      for (x = 0; x < B; x++) {
         *tmpx++ = *tmpa++;
         *tmpy++ = *tmpb++;
      }

      tmpx = x1.dp;
      for (x = B; x < a->used; x++) {
         *tmpx++ = *tmpa++;
      }

      tmpy = y1.dp;
      for (x = B; x < b->used; x++) {
         *tmpy++ = *tmpb++;
      }
   }

   /* only need to clamp the lower words since by definition the
    * upper words x1/y1 must have a known number of digits
    */
   mp_clamp(&x0);
   mp_clamp(&y0);

   /* now calc the products x0y0 and x1y1 */
   /* after this x0 is no longer required, free temp [x0==t2]! */
   if (mp_mul(&x0, &y0, &x0y0) != MP_OKAY)
      goto X1Y1;          /* x0y0 = x0*y0 */
   if (mp_mul(&x1, &y1, &x1y1) != MP_OKAY)
      goto X1Y1;          /* x1y1 = x1*y1 */

   /* now calc x1+x0 and y1+y0 */
   if (s_mp_add(&x1, &x0, &t1) != MP_OKAY)
      goto X1Y1;          /* t1 = x1 - x0 */
   if (s_mp_add(&y1, &y0, &x0) != MP_OKAY)
      goto X1Y1;          /* t2 = y1 - y0 */
   if (mp_mul(&t1, &x0, &t1) != MP_OKAY)
      goto X1Y1;          /* t1 = (x1 + x0) * (y1 + y0) */

   /* add x0y0 */
   if (mp_add(&x0y0, &x1y1, &x0) != MP_OKAY)
      goto X1Y1;          /* t2 = x0y0 + x1y1 */
   if (s_mp_sub(&t1, &x0, &t1) != MP_OKAY)
      goto X1Y1;          /* t1 = (x1+x0)*(y1+y0) - (x1y1 + x0y0) */

   /* shift by B */
   if (mp_lshd(&t1, B) != MP_OKAY)
      goto X1Y1;          /* t1 = (x0y0 + x1y1 - (x1-x0)*(y1-y0))<<B */
   if (mp_lshd(&x1y1, B * 2) != MP_OKAY)
      goto X1Y1;          /* x1y1 = x1y1 << 2*B */

   if (mp_add(&x0y0, &t1, &t1) != MP_OKAY)
      goto X1Y1;          /* t1 = x0y0 + t1 */
   if (mp_add(&t1, &x1y1, c) != MP_OKAY)
      goto X1Y1;          /* t1 = x0y0 + t1 + x1y1 */

   /* Algorithm succeeded set the return code to MP_OKAY */
   err = MP_OKAY;

X1Y1:
   mp_clear(&x1y1);
X0Y0:
   mp_clear(&x0y0);
T1:
   mp_clear(&t1);
Y1:
   mp_clear(&y1);
Y0:
   mp_clear(&y0);
X1:
   mp_clear(&x1);
X0:
   mp_clear(&x0);
LBL_ERR:
   return err;
}
#endif
#ifdef S_MP_MUL_DIGS_C
/* multiplies |a| * |b| and only computes upto digs digits of result
 * HAC pp. 595, Algorithm 14.12  Modified so you can control how
 * many digits of output are created.
 */
mp_err s_mp_mul_digs(const mp_int *a, const mp_int *b, mp_int *c, int digs)
{
   mp_int  t;
   mp_err  err;
   int     pa, pb, ix, iy;
   mp_digit u;
   mp_word r;
   mp_digit tmpx, *tmpt, *tmpy;

   /* can we use the fast multiplier? */
   if ((digs < MP_WARRAY) &&
       (MP_MIN(a->used, b->used) < MP_MAXFAST)) {
      return s_mp_mul_digs_fast(a, b, c, digs);
   }

   if ((err = mp_init_size(&t, digs)) != MP_OKAY) {
      return err;
   }
   t.used = digs;

   /* compute the digits of the product directly */
   pa = a->used;
   for (ix = 0; ix < pa; ix++) {
      /* set the carry to zero */
      u = 0;

      /* limit ourselves to making digs digits of output */
      pb = MP_MIN(b->used, digs - ix);

      /* setup some aliases */
      /* copy of the digit from a used within the nested loop */
      tmpx = a->dp[ix];

      /* an alias for the destination shifted ix places */
      tmpt = t.dp + ix;

      /* an alias for the digits of b */
      tmpy = b->dp;

      /* compute the columns of the output and propagate the carry */
      for (iy = 0; iy < pb; iy++) {
         /* compute the column as a mp_word */
         r       = (mp_word)*tmpt +
                   ((mp_word)tmpx * (mp_word)*tmpy++) +
                   (mp_word)u;

         /* the new column is the lower part of the result */
         *tmpt++ = (mp_digit)(r & (mp_word)MP_MASK);

         /* get the carry word from the result */
         u       = (mp_digit)(r >> (mp_word)MP_DIGIT_BIT);
      }
      /* set carry if it is placed below digs */
      if ((ix + iy) < digs) {
         *tmpt = u;
      }
   }

   mp_clamp(&t);
   mp_exch(&t, c);

   mp_clear(&t);
   return MP_OKAY;
}
#endif
#ifdef S_MP_MUL_DIGS_FAST_C
/* Fast (comba) multiplier
 *
 * This is the fast column-array [comba] multiplier.  It is
 * designed to compute the columns of the product first
 * then handle the carries afterwards.  This has the effect
 * of making the nested loops that compute the columns very
 * simple and schedulable on super-scalar processors.
 *
 * This has been modified to produce a variable number of
 * digits of output so if say only a half-product is required
 * you don't have to compute the upper half (a feature
 * required for fast Barrett reduction).
 *
 * Based on Algorithm 14.12 on pp.595 of HAC.
 *
 */
mp_err s_mp_mul_digs_fast(const mp_int *a, const mp_int *b, mp_int *c, int digs)
{
   int      olduse, pa, ix, iz;
   mp_err   err;
   mp_digit W[MP_WARRAY];
   mp_word  _W;

   /* grow the destination as required */
   if (c->alloc < digs) {
      if ((err = mp_grow(c, digs)) != MP_OKAY) {
         return err;
      }
   }

   /* number of output digits to produce */
   pa = MP_MIN(digs, a->used + b->used);

   /* clear the carry */
   _W = 0;
   for (ix = 0; ix < pa; ix++) {
      int      tx, ty;
      int      iy;
      mp_digit *tmpx, *tmpy;

      /* get offsets into the two bignums */
      ty = MP_MIN(b->used-1, ix);
      tx = ix - ty;

      /* setup temp aliases */
      tmpx = a->dp + tx;
      tmpy = b->dp + ty;

      /* this is the number of times the loop will iterrate, essentially
         while (tx++ < a->used && ty-- >= 0) { ... }
       */
      iy = MP_MIN(a->used-tx, ty+1);

      /* execute loop */
      for (iz = 0; iz < iy; ++iz) {
         _W += (mp_word)*tmpx++ * (mp_word)*tmpy--;

      }

      /* store term */
      W[ix] = (mp_digit)_W & MP_MASK;

      /* make next carry */
      _W = _W >> (mp_word)MP_DIGIT_BIT;
   }

   /* setup dest */
   olduse  = c->used;
   c->used = pa;

   {
      mp_digit *tmpc;
      tmpc = c->dp;
      for (ix = 0; ix < pa; ix++) {
         /* now extract the previous digit [below the carry] */
         *tmpc++ = W[ix];
      }

      /* clear unused digits [that existed in the old copy of c] */
      MP_ZERO_DIGITS(tmpc, olduse - ix);
   }
   mp_clamp(c);
   return MP_OKAY;
}
#endif
#ifdef S_MP_REVERSE_C
/* reverse an array, used for radix code */
void s_mp_reverse(unsigned char *s, int len)
{
   int     ix, iy;
   unsigned char t;

   ix = 0;
   iy = len - 1;
   while (ix < iy) {
      t     = s[ix];
      s[ix] = s[iy];
      s[iy] = t;
      ++ix;
      --iy;
   }
}
#endif
#ifdef S_MP_SUB_C
/* low level subtraction (assumes |a| > |b|), HAC pp.595 Algorithm 14.9 */
mp_err s_mp_sub(const mp_int *a, const mp_int *b, mp_int *c)
{
   int    olduse, min, max;
   mp_err err;

   /* find sizes */
   min = b->used;
   max = a->used;

   /* init result */
   if (c->alloc < max) {
      if ((err = mp_grow(c, max)) != MP_OKAY) {
         return err;
      }
   }
   olduse = c->used;
   c->used = max;

   {
      mp_digit u, *tmpa, *tmpb, *tmpc;
      int i;

      /* alias for digit pointers */
      tmpa = a->dp;
      tmpb = b->dp;
      tmpc = c->dp;

      /* set carry to zero */
      u = 0;
      for (i = 0; i < min; i++) {
         /* T[i] = A[i] - B[i] - U */
         *tmpc = (*tmpa++ - *tmpb++) - u;

         /* U = carry bit of T[i]
          * Note this saves performing an AND operation since
          * if a carry does occur it will propagate all the way to the
          * MSB.  As a result a single shift is enough to get the carry
          */
         u = *tmpc >> (MP_SIZEOF_BITS(mp_digit) - 1u);

         /* Clear carry from T[i] */
         *tmpc++ &= MP_MASK;
      }

      /* now copy higher words if any, e.g. if A has more digits than B  */
      for (; i < max; i++) {
         /* T[i] = A[i] - U */
         *tmpc = *tmpa++ - u;

         /* U = carry bit of T[i] */
         u = *tmpc >> (MP_SIZEOF_BITS(mp_digit) - 1u);

         /* Clear carry from T[i] */
         *tmpc++ &= MP_MASK;
      }

      /* clear digits above used (since we may not have grown result above) */
      MP_ZERO_DIGITS(tmpc, olduse - c->used);
   }

   mp_clamp(c);
   return MP_OKAY;
}

#endif
