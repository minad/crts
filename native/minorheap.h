#pragma once

#include "blockalloc.h"

/**
 * Data structure containing everything
 * needed for the nursery block heap.
 *
 * Each ChiProcessor holds its own nursery
 * including a separate ChiBlockManager
 * for low contention and to improve locality.
 *
 * There are exist two allocators on top of the
 * memory managers, one used only for fast in-band
 * pointer bump allocation. The other allocator
 * is used for out-of-band allocation from native
 * functions, e.g., for array, buffer, bigint and
 * string memory allocations.
 */
typedef struct {
    ChiBlockAlloc   outOfBandAlloc;
    ChiBlockAlloc   bumpAlloc;
    ChiBlockManager manager;
} ChiNursery;

/**
 * Data structure containing everything needed for
 * the tenured block heap. There exists just
 * one tenured block heap shared by all processors.
 *
 * There is one allocator per generation to
 * support generational scavenging.
 * The allocators are _not_ thread-safe.
 * This is not a problem (yet), since
 * allocation happens only in the single-threaded scavenger.
 * If we are moving to multi-threaded scavenging,
 * we need separate allocators per generation and per
 * scavenger worker.
 */
typedef struct {
    ChiBlockManager manager;
    ChiBlockAlloc   alloc[CHI_GEN_MAX - 1];       ///< One allocator per generation to support generational scavenging
    ChiBlockAlloc   cheneyAlloc[CHI_GEN_MAX - 1]; ///< Only used in the scavenger to support Cheney-style scanning
} ChiTenure;

typedef struct ChiProcessor_ ChiProcessor;
typedef struct ChiRuntime_ ChiRuntime;

void chiTenureSetup(ChiTenure*, size_t, size_t, uint32_t);
void chiTenureDestroy(ChiTenure*, uint32_t);
void chiNurserySetup(ChiNursery*, size_t, size_t, size_t);
void chiNurseryDestroy(ChiNursery*);

CHI_INL CHI_WU ChiWord* chiNurseryPtr(ChiNursery* n) {
    return n->bumpAlloc.block->ptr;
}

CHI_INL CHI_WU ChiWord* chiNurseryLimit(ChiNursery* n) {
    return n->bumpAlloc.block->end;
}

CHI_INL void chiNurserySetPtr(ChiNursery* n, ChiWord* ptr) {
    CHI_ASSERT((uint8_t*)ptr >= n->bumpAlloc.block->marked);
    CHI_ASSERT(ptr <= chiNurseryLimit(n));
    n->bumpAlloc.block->ptr = ptr;
}

CHI_INL void chiNurseryGrow(ChiNursery* n) {
    chiBlockFresh(&n->bumpAlloc);
}
