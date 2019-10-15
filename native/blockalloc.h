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
 *       Every processor maintains its own instances.
 */
typedef struct {
    ChiBlockManager* manager;
    ChiBlockList     usedList;
    ChiBlock*        block;
    size_t           limit;
} ChiBlockAlloc;

CHI_INTERN void chiBlockAllocLimitReached(ChiBlockManager*);
CHI_INTERN void chiBlockAllocSetup(ChiBlockAlloc*, ChiBlockManager*, size_t);
CHI_INTERN void chiBlockAllocDestroy(ChiBlockAlloc*);
CHI_INTERN CHI_RET_NONNULL ChiBlock* chiBlockAllocFresh(ChiBlockAlloc*);
CHI_INTERN CHI_RET_NONNULL ChiBlock* chiBlockAllocTake(ChiBlockAlloc*);

/**
 * Bump allocation which never fails
 */
CHI_INL CHI_WU void* chiBlockAllocNew(ChiBlockAlloc* ba, size_t s) {
    CHI_ASSERT(s <= CHI_MAX_UNPINNED);
    ChiBlock* b = ba->block;
    CHI_ASSERT(b);
    if (CHI_UNLIKELY(b->alloc.ptr + s > b->alloc.end))
        b = chiBlockAllocFresh(ba);
    ChiWord* p = b->alloc.ptr;
    CHI_ASSERT(b->alloc.ptr + s <= b->alloc.end);
    chiPoisonObject(p, s);
    b->alloc.ptr += s;
    return p;
}

CHI_INL CHI_WU size_t chiBlockAllocIndex(ChiBlock* b, void* p) {
    CHI_ASSERT_POISONED((uintptr_t)b->owner, _chiDebugData.ownerMinor);
    //CHI_ASSERT((ChiWord*)p >= b->alloc.base); base is invalid during scavenging
    CHI_ASSERT((ChiWord*)p > b->alloc.forward);
    CHI_ASSERT((ChiWord*)p < b->alloc.end);
    CHI_ASSERT((uintptr_t)p % CHI_WORDSIZE == 0);
    return (size_t)((ChiWord*)p - (const ChiWord*)b);
}

CHI_INL CHI_WU bool chiBlockAllocSetForward(ChiBlock* b, void* p) {
    size_t idx = chiBlockAllocIndex(b, p);
    if (chiBitGet((const uintptr_t*)b->alloc.forward, idx))
        return false;
    chiBitSet((uintptr_t*)b->alloc.forward, idx, true);
    return true;
}

CHI_INL CHI_WU bool chiBlockAllocGetForward(ChiBlock* b, void* p) {
    return chiBitGet((const uintptr_t*)b->alloc.forward, chiBlockAllocIndex(b, p));
}

CHI_INL CHI_WU ChiBlock* chiBlockAllocBlock(void* p, uintptr_t blockMask) {
    ChiBlock* b = (ChiBlock*)((uintptr_t)p & blockMask);
    CHI_ASSERT_POISONED(b->canary, CHI_POISON_BLOCK_CANARY);
    CHI_ASSERT_POISONED((uintptr_t)b->owner, _chiDebugData.ownerMinor);
    return b;
}

CHI_INL CHI_WU bool chiBlockAllocSetForwardMasked(void* p, uintptr_t blockMask) {
    return chiBlockAllocSetForward(chiBlockAllocBlock(p, blockMask), p);
}

CHI_INL CHI_WU bool chiBlockAllocGetForwardMasked(void* p, uintptr_t blockMask) {
    return chiBlockAllocGetForward(chiBlockAllocBlock(p, blockMask), p);
}

CHI_INL CHI_WU ChiWord* chiBlockAllocBase(ChiBlock* b, size_t blockSize) {
    CHI_ASSERT_POISONED((uintptr_t)b->owner, _chiDebugData.ownerMinor);
    return b->alloc.forward + blockSize / (CHI_WORDSIZE * CHI_WORDBITS);
}
