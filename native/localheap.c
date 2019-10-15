#include "localheap.h"

void chiLocalHeapSetup(ChiLocalHeap* h, size_t block, size_t chunk, size_t limit, ChiRuntime* rt) {
    chiBlockManagerSetup(&h->manager, block, chunk, false, rt);

    chiBlockAllocSetup(&h->agedAlloc, &h->manager, 0);
    chiBlockAllocSetup(&h->nursery.alloc, &h->manager, limit);
    h->nursery.bump = chiBlockAllocTake(&h->nursery.alloc);

    chiPromotedVecInit(&h->promoted.vec, &h->manager);
    chiBlockVecInit(&h->majorDirty, &h->manager);
}

void chiLocalHeapDestroy(ChiLocalHeap* h) {
    chiBlockAllocDestroy(&h->nursery.alloc);
    chiBlockAllocDestroy(&h->agedAlloc);
    chiPromotedVecFree(&h->promoted.vec);

    chiBlockVecFree(&h->majorDirty);
    chiBlockManagerDestroy(&h->manager);

    CHI_POISON_STRUCT(h, CHI_POISON_DESTROYED);
}
