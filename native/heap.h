#pragma once

#include "chunk.h"
#include "num.h"
#include "object.h"
#include "system.h"

typedef struct ChiProcessor_ ChiProcessor;
typedef struct ChiHeap_ ChiHeap;
typedef union ChiHeapPage_ ChiHeapPage;
typedef struct ChiHeapClass_ ChiHeapClass;
typedef struct ChiHeapSegmentLink_ ChiHeapSegmentLink;
struct ChiHeapSegmentLink_ { ChiHeapSegmentLink *prev, *next; };

#define CHI_HEAP_PAGE_FREE (1U << 31)

/**
 * Each heap page contains objects of a fixed size.
 */
union ChiHeapPage_ {
    uint32_t objectSize;
    uint32_t nextIndex;
};

/**
 * The heap consists of multiple segments.
 * The segments are split in pages.
 * Each segment has one associated ChiChunk.
 */
typedef struct {
    ChiHeapClass*      cls;
    ChiChunk*          chunk;
    ChiHeapSegmentLink link;
    uint32_t           sweepIndex, endIndex;
    bool               alive;
    ChiHeapPage        *firstPage, *freePageList;
    ChiObject          *freeObjectList[];
} ChiHeapSegment;

#define LIST_LENGTH
#define LIST_PREFIX chiHeapSegmentList
#define LIST_ELEM   ChiHeapSegment
#include "generic/list.h"

struct ChiHeapClass_ {
    CHI_IF(CHI_COUNT_ENABLED, uint64_t allocWords;)
    size_t             total;
    ChiHeapSegmentList free, full;
    size_t             limitInit;
    uint32_t           limitFull;
    uint32_t           segmentWords, pageWords, pagesPerSegment, bins;
    bool               linear;
};

typedef struct {
    CHI_IF(CHI_COUNT_ENABLED, uint64_t allocWords;)
    ChiHeapSegment* segment;
} ChiHeapSegmentHandle;

/**
 * Heap handle used for lockless allocations. Each processor
 * manages its own heap handle.
 */
typedef struct {
    ChiHeap*             heap;
    ChiHeapSegmentHandle small, medium;
    ChiColor             allocColor;
} ChiHeapHandle;

typedef struct {
    CHI_IF(CHI_COUNT_ENABLED, uint64_t allocWords;)
    ChiChunkList      list;
    size_t            totalWords;
    size_t            limitAlloc;
    size_t            limitInit;
    uint32_t          limitAllocPercent;
} ChiHeapLarge;

/**
 * Heap datastructure. The heap is locked by a central mutex.
 * For allocations, a heap handle must be acquired.
 */
struct ChiHeap_ {
    ChiMutex          mutex;
    ChiHeapClass      small, medium;
    ChiHeapLarge      large;
};

CHI_INTERN void chiHeapSetup(ChiHeap*, size_t, size_t, size_t, size_t, uint32_t, size_t);
CHI_INTERN void chiHeapDestroy(ChiHeap*);
CHI_INTERN void chiHeapUsage(ChiHeap*, ChiHeapUsage*);
CHI_INTERN void chiHeapSegmentListFree(ChiHeap*, ChiHeapSegmentList*);
CHI_INTERN void chiHeapChunkListFree(ChiHeap*, ChiChunkList*);

CHI_INTERN void chiHeapSweepAcquire(ChiHeap*, ChiChunkList*, ChiHeapSegmentList*);
CHI_INTERN void chiHeapSweepReleaseSegment(ChiHeap*, ChiHeapSegment*);
CHI_INTERN void chiHeapSweepReleaseLarge(ChiHeap*, ChiChunkList*, size_t);

CHI_INTERN CHI_WU void* chiHeapHandleNew(ChiHeapHandle*, size_t, ChiNewFlags);
CHI_INTERN CHI_WU void* chiHeapHandleNewSmall(ChiHeapHandle*, size_t, ChiNewFlags);
CHI_INTERN void chiHeapHandleSetup(ChiHeapHandle*, ChiHeap*, ChiColor);
CHI_INTERN void chiHeapHandleDestroy(ChiHeapHandle*);

// heap callouts
CHI_INTERN void chiHeapLimitReached(ChiHeap*);
CHI_INTERN CHI_WU ChiChunk* chiHeapChunkNew(ChiHeap*, size_t, ChiNewFlags);
CHI_INTERN void chiHeapChunkFree(ChiHeap*, ChiChunk*);
