#pragma once

#include "blockalloc.h"
#include "heap.h"
#include "objvec.h"

typedef struct {
    Chili from, to;
} ChiPromoted;

#define VEC_TYPE   ChiPromoted
#define VEC_PREFIX chiPromotedVec
#define VEC        ChiPromotedVec
#include "generic/blockvec.h"

typedef struct {
    Chili obj;
    size_t idx;
} ChiCard;

#define VEC_TYPE   ChiCard
#define VEC_PREFIX chiCardVec
#define VEC        ChiCardVec
#include "generic/blockvec.h"

/**
 * Data structure containing everything needed for the
 * minor block heap, which is processor local.
 *
 * For the nursery, there exists one allocators on top of the
 * memory manager. The allocator is used for out-of-band allocation
 * from native functions, e.g., for bigint and
 * string memory allocations. Furthermore there is a block taken
 * from the allocator for fast in-band pointer bump allocation.
 *
 * For the aged generations, there exists another allocator.
 *
 * Heap invariants:
 *
 *     * Different Chili values never point to the same heap object.
 *
 *     * There are no pointers from major-shared to major-local.
 *       There is one exception however, the thread to stack pointer,
 *       since stacks live in major-local, when they are active.
 *       This exception is not problematic, since stacks are runtime
 *       internal. If a stack is about to be used, it must be ensured that
 *       the stack is either living in major-shared or is living
 *       in major-local, owned by the current processor.
 *       the current owner of the stack is indeed the current processor.
 *
 *     * There are no pointers from major-shared to minor heaps.
 *
 *     * There are no pointers between different minor heaps
 *
 *     * Dirty lists contain objects from major-local heaps
 *       belonging to the current processor.
 *
 *     * Objects living in the major-local generation
 *       pointing to an object living in the minor heap
 *       will be registered in a dirty list.
 *
 *     * Weak tricolor invariant for the major-shared heap:
 *       For every white object, pointed to by a black object, there exists
 *       another path to this object, originating from a gray object.
 *       This property is sometimes called gray-protected.
 *       This means it is ensured, that the white object will be scanned
 *       and marked black in the end.
 *
 *     * Gray lists only contain major-shared objects
 *
 *     * Mutable objects like Array and Buffer are never allocated in the minor heap
 */
typedef struct {
    ChiBlockManager     manager;
    struct {
        ChiPromotedVec  objects;
        CHI_IF(CHI_COUNT_ENABLED, ChiPromoteStats stats;)
    } promoted;
    struct {
        ChiBlockAlloc   alloc;
        ChiBlock*       bump;
        size_t          limit;
    } nursery;
    ChiBlockAlloc       agedAlloc;
    struct {
        ChiObjVec       objects;
        ChiCardVec      cards;
    } dirty;
} ChiLocalHeap;

typedef struct ChiRuntime_ ChiRuntime;
CHI_INTERN void chiLocalHeapSetup(ChiLocalHeap*, size_t, size_t, size_t, ChiRuntime*);
CHI_INTERN void chiLocalHeapDestroy(ChiLocalHeap*);

CHI_INL CHI_WU ChiWord* chiLocalHeapBumpBase(ChiLocalHeap* h) {
    return h->nursery.bump->alloc.base;
}

CHI_INL CHI_WU ChiWord* chiLocalHeapBumpLimit(ChiLocalHeap* h) {
    return h->nursery.bump->alloc.end;
}

CHI_INL void chiLocalHeapBumpGrow(ChiLocalHeap* h) {
    h->nursery.bump = chiBlockAllocTake(&h->nursery.alloc);
}
