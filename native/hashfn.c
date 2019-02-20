#include "hashfn.h"

ChiHashIndex chiHashBytes(const void* p, size_t n) {
    return (ChiHashIndex){ CHI_ARCH_BITS == 64 ? xxh64(p, n, 0) : xxh32(p, n, 0) };
}
