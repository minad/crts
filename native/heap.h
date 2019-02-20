#pragma once

#include "object.h"

#define CHI_FOREACH_HEAP_SEGMENT(s, sl) CHI_LIST_FOREACH(ChiHeapSegment, list, s, sl)
#define CHI_FOREACH_HEAP_SEGMENT_NODELETE(s, sl) CHI_LIST_FOREACH_NODELETE(ChiHeapSegment, list, s, sl)

enum {
    _CHI_HEAP_CLASSES    = 2,
};

typedef struct ChiTimeout_ ChiTimeout;
typedef struct ChiProcessor_ ChiProcessor;
typedef struct ChiChunk_ ChiChunk;
typedef struct ChiHeap_ ChiHeap;

/**
 * Zones consist of segments.
 * Each segment has one associated ChiChunk.
 * The unused objects are kept in bins for
 * each power of two size.
 */
typedef struct {
    ChiChunk*  chunk;
    ChiWord*   sweep;
    bool       live;
    ChiList    list;
    ChiObject* bin[0];
} ChiHeapSegment;

/**
 * Zone descriptor containing the configuration
 * of a zone, number of bins, chunk size etc.
 */
typedef struct {
    size_t   chunk, max;
    uint32_t off, bins, sub;
} ChiHeapDesc;

/**
 * Heap size class. There are six zones,
 * containing lists of segments (free, full, swept free, swept full, sweep, used).
 */
typedef struct {
    ChiHeapClassUsage usage;
    ChiHeapDesc       desc;
    union {
        struct { ChiList free, full, sweptFree, sweptFull, sweep, used; } zone;
        ChiList izone[6];
    };
} ChiHeapClass;

typedef struct {
    ChiList*        zone;
    ChiHeapSegment* segment;
    ChiColor        color;
    size_t          allocWords;
} ChiHeapUsedSegment;

/**
 * Heap handle used for multiple consecutive
 * allocations. Avoids too many heap locks/unlocks
 * during allocation.
 */
typedef struct {
    ChiHeap* heap;
    ChiHeapUsedSegment usedSegment[_CHI_HEAP_CLASSES];
} ChiHeapHandle;

/**
 * Heap datastructure. The heap is locked by a central mutex.
 * For allocations, a heap handle must be acquired.
 *
 * The heap is organized in size classes.
 * Furthermore there is an additional chunk list
 * for large objects.
 */
struct ChiHeap_ {
    ChiHeapClass          cls[_CHI_HEAP_CLASSES]; ///< Heap size classes
    uint32_t              allocLimit;
    ChiMutex              mutex;                  ///< Mutex for handle acquisition
    int32_t               sweep;                  ///< Sweeping
    struct {
        ChiList           list;                   ///< List of large objects
        ChiList           listSwept;              ///< List of swept large objects
        ChiHeapClassUsage usage;
    } large;
};

typedef enum {
    CHI_HEAP_NOFAIL,
    CHI_HEAP_MAYFAIL,
} ChiHeapFail;

void chiHeapSetup(ChiHeap*, const ChiHeapDesc*, const ChiHeapDesc*, uint32_t);
void chiHeapDestroy(ChiHeap*);
CHI_WU Chili chiHeapNewLarge(ChiHeapHandle*, ChiType, size_t, ChiColor, ChiHeapFail);
CHI_WU Chili chiHeapNewSmall(ChiHeapHandle*, ChiType, size_t, ChiColor);
void chiHeapHandleRelease(ChiHeapHandle*);
CHI_WU bool chiHeapHandleReleaseUnlocked(ChiHeapHandle*);
void chiHeapUsage(ChiHeap*, ChiHeapUsage*);

// heap callouts
void chiHeapAllocLimitReached(ChiHeap*);
void chiHeapSweepNotify(ChiHeap*);
CHI_WU ChiChunk* chiHeapChunkNew(ChiHeap*, size_t, ChiHeapFail);
void chiHeapChunkFree(ChiHeap*, ChiChunk*);

void chiSweepSlice(ChiHeap*, ChiSweepStats*, ChiTimeout*);
void chiSweepBegin(ChiHeap*);
CHI_WU bool chiSweepEnd(ChiHeap*);

CHI_INL void chiHeapHandleInit(ChiHeapHandle* ctx, ChiHeap* heap) {
    *ctx = (ChiHeapHandle){ .heap = heap };
}
