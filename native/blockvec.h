#pragma once

#include "blockman.h"

#define CHI_BLOCKVEC_FOREACH(entry, bv)                                 \
    for (Chili _block_mask = (Chili){CHI_MASK((bv)->manager->blockSize)}, \
             _block_ptr = (Chili){(uintptr_t)chiBlockVecBegin(bv)},     \
             entry = CHI_FALSE;                                         \
         chiTrue(_block_ptr) && (entry = *(Chili*)(uintptr_t)CHILI_UN(_block_ptr), 1); \
         _block_ptr = (Chili){(uintptr_t)chiBlockVecNext((bv), (Chili*)(uintptr_t)CHILI_UN(_block_ptr), (uintptr_t)CHILI_UN(_block_mask))})

/**
 * Vector which uses blocks to support fast growth
 */
typedef struct {
    ChiBlockManager* manager;
    ChiBlockList     list;
} ChiBlockVec;

ChiBlock* chiBlockVecGrow(ChiBlockVec*);
void chiBlockVecFree(ChiBlockVec*);
void chiBlockVecShrink(ChiBlockVec*);

CHI_INL void chiBlockVecInit(ChiBlockVec* bv, ChiBlockManager* bm) {
    chiBlockListInit(&bv->list);
    bv->manager = bm;
}

CHI_INL CHI_WU bool chiBlockVecNull(const ChiBlockVec* bv) {
    return chiListNull(&bv->list.list);
}

CHI_INL CHI_WU bool chiBlockVecPop(ChiBlockVec* bv, Chili* val) {
    ChiBlock* b = chiBlockListHead(&bv->list);
    if (!b)
        return false;
    --b->ptr;
    CHI_ASSERT(b->ptr >= b->base);
    *val = *(Chili*)b->ptr;
    if (CHI_UNLIKELY(b->ptr == b->base))
        chiBlockVecShrink(bv);
    return true;
}

CHI_INL Chili* chiBlockVecPush(ChiBlockVec* bv, Chili val) {
    ChiBlock* b = chiBlockListHead(&bv->list);
    if (CHI_UNLIKELY(!b || b->ptr >= b->end))
        b = chiBlockVecGrow(bv);
    Chili* ret = (Chili*)b->ptr++;
    *ret = val;
    return ret;
}

CHI_INL CHI_WU Chili* chiBlockVecBegin(const ChiBlockVec* bv) {
    ChiBlock* b = chiBlockListHead(&bv->list);
    CHI_ASSERT(!b || b->ptr > b->base);
    return b ? (Chili*)b->ptr - 1 : 0;
}

CHI_INL CHI_WU Chili* chiBlockVecNext(const ChiBlockVec* bv, Chili* p, uintptr_t mask) {
    ChiBlock* b = chiBlock(p, mask);
    if (p > (Chili*)b->base)
        return --p;
    return b->list.next != &bv->list.list ? (Chili*)chiOuterBlock(b->list.next)->ptr - 1 : 0;
}

CHI_INL void chiBlockVecJoin(ChiBlockVec* a, ChiBlockVec* b) {
    CHI_ASSERT(a != b);
    CHI_ASSERT(a->manager == b->manager);
    chiBlockListJoin(&a->list, &b->list);
}

CHI_INL void chiBlockVecMove(ChiBlockVec* a, ChiBlockVec* b) {
    CHI_ASSERT(a != b);
    CHI_ASSERT(a->manager == b->manager);
    chiBlockListAppend(&a->list, chiBlockListPop(&b->list));
}
