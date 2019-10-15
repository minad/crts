#include <gmp.h>
#undef Z_BITS
#define Z_BITS GMP_NUMB_BITS
#define z_digit mp_limb_t
#define z_size mp_size_t
#define zd_add mpn_add
#define zd_add_1 mpn_add_1
#define zd_add_n mpn_add_n
#define zd_addmul_1 mpn_addmul_1
#define zd_and_n mpn_and_n
#define zd_andnot_n mpn_andn_n
#define zd_cmp mpn_cmp
#define zd_cpy mpn_copyi
#define zd_or_n mpn_ior_n
#define zd_shl mpn_lshift
#define zd_mul mpn_mul
#define zd_mul_1 mpn_mul_1
#define zd_shr mpn_rshift
#define zd_sqr mpn_sqr
#define zd_sub mpn_sub
#define zd_sub_1 mpn_sub_1
#define zd_sub_n mpn_sub_n
#define zd_divmod(q,r,u,m,v,n) ({ mpn_tdiv_qr((q),(r),0,(u),(m),(v),(n)); true; })
#define zd_xor_n mpn_xor_n
#define zd_zero mpn_zero
