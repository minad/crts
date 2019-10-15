#include <chili/prim.h>
#include "test.h"

Chili int63Add(Chili, Chili);
CHI_NOINL Chili int63Add(Chili x, Chili y) {
    return chiFromInt64(chi_Prim_int64Shr(chi_Prim_int64Shl(chi_Prim_int64Add(chiToInt64(x), chiToInt64(y)), 1), 1));
}

Chili uint63Add(Chili, Chili);
CHI_NOINL Chili uint63Add(Chili x, Chili y) {
    return chiFromUInt64(chi_Prim_uint64And(chi_Prim_uint64Add(chiToUInt64(x), chiToUInt64(y)), 0x7FFFFFFFFFFFFFFFUL));
}

TEST(Int63) {
    ASSERTEQ(chiToInt64(int63Add(chiFromInt64(1), chiFromInt64(-2))), -1);
    ASSERTEQ(chiToUInt64(uint63Add(chiFromUInt64(1), chiFromUInt64(2))), 3);
}
