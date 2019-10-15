#include "gc.h"
#include "timeout.h"

static ChiHeapSegment* sweepResetSegment(ChiHeapSegment* s) {
    s->sweepIndex = s->endIndex;
    s->freePageList = 0;
    s->alive = false;
    memset(s->freeObjectList, 0, sizeof (void*) * s->cls->bins);
    return s;
}

static void sweepTake(ChiGC* gc, ChiHeapSegment** segment, ChiChunkList* largeList) {
    CHI_LOCK_MUTEX(&gc->mutex);
    if (!chiHeapSegmentListNull(&gc->sweep.segmentPartial))
        *segment = chiHeapSegmentListPop(&gc->sweep.segmentPartial);
    else if (!chiHeapSegmentListNull(&gc->sweep.segmentUnswept))
        *segment = sweepResetSegment(chiHeapSegmentListPop(&gc->sweep.segmentUnswept));
    else if (!chiChunkListNull(&gc->sweep.largeList))
        chiChunkListJoin(largeList, &gc->sweep.largeList);
    else
        chiTrigger(&gc->sweep.needed, false);
}

static bool pageSweep(ChiHeapPage* page, ChiObject* start, ChiObject* obj,
                      ChiObject** freeObjectList, ChiColorState colorState,
                      ChiSweepClassStats* stats) {
    CHI_NOWARN_UNUSED(stats);
    bool alive = false;
    uint32_t objectSize = page->objectSize;
    // Sweep from the end in order to obtain a free list with increasing addresses
    while (obj >= start) {
        if (chiObjectGarbage(colorState, obj)) {
            chiCountObject(stats->garbage, objectSize);
            obj->free.next = *freeObjectList;
            *freeObjectList = obj;
        } else {
            alive = true;
            chiCountObject(stats->alive, objectSize);
        }
        obj -= objectSize;
    }
    return alive;
}

static void segmentSweep(ChiHeapSegment* s, ChiColorState colorState,
                         ChiSweepClassStats* stats, ChiTimeout* timeout) {
    ChiHeapClass* cls = s->cls;
    // Sweep segment backward in order to get a page free list with increasing addresses
    while (s->sweepIndex > 0 && chiTimeoutTick(timeout)) {
        --s->sweepIndex;
        ChiHeapPage* page = s->firstPage + s->sweepIndex;
        if (page->nextIndex & CHI_HEAP_PAGE_FREE) {
            // insert unused page in list of free pages
            page->nextIndex = s->freePageList ? ((uint32_t)(s->freePageList - s->firstPage) | CHI_HEAP_PAGE_FREE) : UINT32_MAX;
            s->freePageList = page;
        } else {
            uint32_t objectSize = page->objectSize,
                bin = cls->linear
                ? objectSize - _CHI_OBJECT_HEADER_SIZE - 1
                : chiLog2(objectSize - _CHI_OBJECT_HEADER_SIZE);
            CHI_ASSERT(bin <= cls->bins);

            ChiObject* freeObjectList = s->freeObjectList[bin],
                *start = (ChiObject*)s->chunk->start + s->sweepIndex * cls->pageWords,
                *last = start + (cls->pageWords / objectSize) * objectSize - objectSize;
            if (pageSweep(page, start, last, &freeObjectList, colorState, stats)) {
                s->freeObjectList[bin] = freeObjectList;
                s->alive = true;
            } else {
                // insert unused page in list of free pages
                page->nextIndex = s->freePageList ? ((uint32_t)(s->freePageList - s->firstPage) | CHI_HEAP_PAGE_FREE) : UINT32_MAX;
                s->freePageList = page;
            }
        }
    }
}

static size_t largeSweep(ChiHeap* heap, ChiChunkList* list, ChiColorState colorState, ChiSweepClassStats* stats) {
    CHI_NOWARN_UNUSED(stats);
    size_t freed = 0;
    CHI_FOREACH_CHUNK (c, list) {
        ChiObject* obj = (ChiObject*)c->start;
        size_t size = c->size / CHI_WORDSIZE;
        if (chiObjectGarbage(colorState, obj)) {
            freed += size;
            chiCountObject(stats->garbage, size);
            chiChunkListDelete(c);
            chiHeapChunkFree(heap, c);
        } else {
            chiCountObject(stats->alive, size);
        }
    }
    return freed;
}

void chiSweepSlice(ChiHeap* heap, ChiGC* gc, ChiColorState colorState, ChiSweepStats* pstats, ChiTimeout* timeout) {
    ChiSweepClassStats largeStats = {}, smallStats = {}, mediumStats = {};
    while (chiTimeoutTick(timeout)) {
        ChiHeapSegment* s = 0;
        ChiChunkList largeList = CHI_LIST_INIT(largeList);
        sweepTake(gc, &s, &largeList);
        if (s) {
            segmentSweep(s, colorState, s->cls == &heap->small ? &smallStats : &mediumStats, timeout);
            if (s->sweepIndex) {
                CHI_LOCK_MUTEX(&gc->mutex);
                chiHeapSegmentListAppend(&gc->sweep.segmentPartial, s);
                chiTrigger(&gc->sweep.needed, true);
            } else {
                chiHeapSweepReleaseSegment(heap, s);
            }
        } else if (!chiChunkListNull(&largeList)) {
            size_t freed = largeSweep(heap, &largeList, colorState, &largeStats);
            chiHeapSweepReleaseLarge(heap, &largeList, freed);
        } else {
            break;
        }
    }
    *pstats = (ChiSweepStats){ .large = largeStats, .medium = mediumStats, .small = smallStats };
}
