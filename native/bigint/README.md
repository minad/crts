Minimal big integer library
===========================

API:

~~~ c
typedef struct {
    z_digit* d;  // digits
    z_size size; // number of digits
    z_size alloc;  // allocated digits
    bool neg;    // integer is negative
    bool err;    // true if allocation failed, error is propagated
} z_int;

// constants
const z_int z_err;
const z_int z_zero;

// comparison
int32_t z_cmp(z_int a, z_int a);

// free integers
void z_free(z_int a);
void z_frees(...);

// unary functions
z_int z_neg(z_int a);
z_int z_not(z_int a);
z_int z_abs(z_int a);

// addition and subtraction
z_int z_add(z_int a, z_int b);
z_int z_add_1(z_int a, z_digit b);
z_int z_sub(z_int a, z_int b);
z_int z_sub_1(z_int a, z_digit b);

// multiplication
z_int z_mul(z_int a, z_int b);

// division
void z_divmod(z_int* dp, z_int* mp, z_int a, z_int b); // precondition: b!=z_zero
void z_quorem(z_int* qp, z_int* rp, z_int a, z_int b); // precondition: b!=z_zero
z_int z_div(z_int a, z_int b); // precondition: b!=z_zero
z_int z_mod(z_int a, z_int b); // precondition: b!=z_zero
z_int z_quo(z_int a, z_int b); // precondition: b!=z_zero
z_int z_rem(z_int a, z_int b); // precondition: b!=z_zero

// two-complements bitwise operations
z_int z_and(z_int a, z_int b);
z_int z_or(z_int a, z_int b);
z_int z_xor(z_int a, z_int b);
z_int z_shl(z_int a, uint16_t b);
z_int z_shr(z_int a, uint16_t b);

// conversion
double z_to_d(z_int a); // precondition: a.err==false
uint64_t z_to_u64(z_int a); // precondition: a.err==false
z_int z_from_b(const uint8_t *buf, size_t size);
z_int z_from_d(double b);
z_int z_from_i64(int64_t b);
z_int z_from_u64(uint64_t b);
~~~

Configuration:

~~~ c
#define Z_BITS      64 // 8, 16, 32, 64
#define Z_GMP       0  // 0 = do not use GMP, 1 = use GMP mpn functions
#define Z_SCRATCH   64 // scratch space on the stack in digits
#define Z_ASSERT(x) assert(x)
#define Z_ALLOC(x)  malloc(x)
#define Z_FREE(x)   free(x)
#include "z.h"
~~~

**Note**: It suffices to check the `err` field of the resulting integers.
The error is propagated, e.g., `a.err || b.err ==> z_add(a, b).err`.
The field MUST be checked before calling `z_to_d` or `z_to_u64`.

License
-------

MIT
