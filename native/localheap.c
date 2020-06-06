#include "localheap.h"

void chiLocalHeapSetup(ChiLocalHeap* h, size_t block, size_t chunk, size_t limit, ChiRuntime* rt) {
    h->nursery.limit = limit;

    chiBlockManagerSetup(&h->manager, block, chunk, false, rt);
    h->manager.allocHookEnabled = true;

    chiBlockAllocSetup(&h->agedAlloc, &h->manager);
    chiBlockAllocSetup(&h->nursery.alloc, &h->manager);
    h->nursery.bump = chiBlockAllocTake(&h->nursery.alloc);

    chiPromotedVecInit(&h->promoted.objects, &h->manager);
    chiObjVecInit(&h->dirty.objects, &h->manager);
    chiCardVecInit(&h->dirty.cards, &h->manager);
}

void chiLocalHeapDestroy(ChiLocalHeap* h) {
    chiBlockAllocDestroy(&h->nursery.alloc);
    chiBlockAllocDestroy(&h->agedAlloc);

    chiPromotedVecFree(&h->promoted.objects);
    chiObjVecFree(&h->dirty.objects);
    chiCardVecFree(&h->dirty.cards);

    chiBlockManagerDestroy(&h->manager);
    CHI_POISON_STRUCT(h, CHI_POISON_DESTROYED);
}
