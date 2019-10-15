#include "hashfn.h"

ChiHash chiHashBytes(const void* p, size_t n) {
    return CHI_WRAP(Hash, CHI_ARCH_32BIT ? xxh32(p, n, 0) : (size_t)xxh64(p, n, 0));
}
