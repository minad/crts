#pragma once

#include "blockman.h"

/**
 * A block allocator is a thin layer
 * on top of the ChiBlockManager,
 * which is responsible for allocations of objects,
 * which live in multiple generations.
 * The blocks used by the block allocator
 * are returned to the block manager after
 * a scavenger garbage collection.
 *
 * @note The block allocator is not thread safe.
 *       Thus every ChiProcessor owns a separate instance.
 */
typedef struct {
    ChiBlockManager* manager;
    ChiBlockList     used;
    ChiBlockList     old;
    ChiBlock*        block;
    ChiGen           gen;
} ChiBlockAlloc;

void chiBlockAllocSetup(ChiBlockAlloc*, ChiGen, ChiBlockManager*);
void chiBlockAllocDestroy(ChiBlockAlloc*);
CHI_RET_NONNULL ChiBlock* chiBlockFresh(ChiBlockAlloc*);

/**
 * Bump allocation which never fails
 */
CHI_INL CHI_WU Chili chiBlockNew(ChiBlockAlloc* ba, ChiType t, size_t s) {
    CHI_ASSERT(s <= CHI_MAX_UNPINNED);
    ChiBlock* b = ba->block;
    CHI_ASSERT(b);
    if (CHI_UNLIKELY(b->ptr + s > b->end))
        b = chiBlockFresh(ba);
    ChiWord* p = b->ptr;
    CHI_ASSERT(b->ptr + s <= b->end);
    chiPoisonObject(p, s);
    b->ptr += s;
    return chiWrap(p, s, t);
}
