#include "runtime.h"

void chiTenureSetup(ChiTenure* t, size_t block, size_t chunk, uint32_t gens) {
    chiBlockManagerSetup(&t->manager, block, chunk, 0);
    for (uint32_t gen = gens; gen --> 1;)
        chiBlockAllocSetup(t->alloc + gen - 1, (ChiGen)gen, &t->manager);
}

void chiTenureDestroy(ChiTenure* t, uint32_t gens) {
    for (uint32_t gen = gens; gen --> 1;)
        chiBlockAllocDestroy(t->alloc + gen - 1);
    chiBlockManagerDestroy(&t->manager);
    chiPoisonMem(t, CHI_POISON_DESTROYED, sizeof (ChiTenure));
}

void chiNurserySetup(ChiNursery* n, size_t block, size_t chunk, size_t limit) {
    chiBlockManagerSetup(&n->manager, block, chunk, limit);
    chiBlockAllocSetup(&n->outOfBandAlloc, CHI_GEN_NURSERY, &n->manager);
    chiBlockAllocSetup(&n->bumpAlloc, CHI_GEN_NURSERY, &n->manager);
}

void chiNurseryDestroy(ChiNursery* n) {
    chiBlockAllocDestroy(&n->outOfBandAlloc);
    chiBlockAllocDestroy(&n->bumpAlloc);
    chiBlockManagerDestroy(&n->manager);
    chiPoisonMem(n, CHI_POISON_DESTROYED, sizeof (ChiNursery));
}
