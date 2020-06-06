#include "heap.h"
#include "mem.h"

#define SMALL_BINS CHI_MAX_UNPINNED

CHI_INL void objectInit(ChiObject* obj, size_t size, ChiColor color, ChiNewFlags flags) {
    CHI_ASSERT(size <= UINT32_MAX);
    obj->size = (uint32_t)size;
    obj->flags = (ChiObjectFlags){.dirty = !(flags & CHI_NEW_CLEAN),
                                  CHI_IF(CHI_SYSTEM_HAS_TASK, .shared = !!(flags & CHI_NEW_SHARED)) };
    atomic_store_explicit(&obj->color, CHI_UN(Color, color), memory_order_relaxed);
    atomic_store_explicit(&obj->lock, 0, memory_order_relaxed);
    CHI_IF(CHI_POISON_ENABLED, atomic_store_explicit(&obj->owner, chiExpectedObjectOwner(), memory_order_relaxed);)
    CHI_ASSERT((atomic_load_explicit(&obj->header, memory_order_relaxed) & 0xFF) == CHI_UN(Color, color));
    chiPoisonObject(obj->payload, size);
}

void chiHeapUsage(ChiHeap* heap, ChiHeapUsage* usage) {
    CHI_LOCK_MUTEX(&heap->mutex);
    usage->small.allocSize = CHI_WORDSIZE * CHI_AND(CHI_COUNT_ENABLED, heap->small.allocWords);
    usage->medium.allocSize = CHI_WORDSIZE * CHI_AND(CHI_COUNT_ENABLED, heap->medium.allocWords);
    usage->large.allocSize = CHI_WORDSIZE * CHI_AND(CHI_COUNT_ENABLED, heap->large.allocWords);
    usage->small.totalSize = CHI_WORDSIZE * heap->small.total * heap->small.segmentWords;
    usage->medium.totalSize = CHI_WORDSIZE * heap->medium.total * heap->medium.segmentWords;
    usage->large.totalSize = CHI_WORDSIZE * heap->large.totalWords;
}

static bool classLimitReached(const ChiHeapClass* cls) {
    return cls->full.length > cls->limitFull * cls->total / 100 && cls->total > cls->limitInit;
}

static void classSetup(ChiHeapClass* cls, size_t segmentSize, size_t pageSize,
                       uint32_t limitFull, size_t limitInit, uint32_t bins, bool linear) {
    cls->segmentWords = (uint32_t)(segmentSize / CHI_WORDSIZE);
    cls->pageWords = (uint32_t)(pageSize / CHI_WORDSIZE);
    cls->pagesPerSegment = cls->segmentWords / cls->pageWords;
    cls->bins = bins;
    cls->linear = linear;
    cls->limitFull = limitFull;
    cls->limitInit = limitFull * (limitInit / segmentSize) / 100;
    chiHeapSegmentListInit(&cls->free);
    chiHeapSegmentListInit(&cls->full);
}

void chiHeapSegmentListFree(ChiHeap* heap, ChiHeapSegmentList* list) {
    while (!chiHeapSegmentListNull(list)) {
        ChiHeapSegment* s = chiHeapSegmentListPop(list);
        chiHeapChunkFree(heap, s->chunk);
        chiFree(s);
    }
}

void chiHeapChunkListFree(ChiHeap* heap, ChiChunkList* list) {
    while (!chiChunkListNull(list))
        chiHeapChunkFree(heap, chiChunkListPop(list));
}

static void classDestroy(ChiHeap* heap, ChiHeapClass* cls) {
    chiHeapSegmentListFree(heap, &cls->free);
    chiHeapSegmentListFree(heap, &cls->full);
}

static void segmentAcquireExisting(ChiHeapClass* cls, ChiHeapSegmentHandle* sh) {
    CHI_ASSERT(!sh->segment);
    if (!chiHeapSegmentListNull(&cls->free))
        sh->segment = chiHeapSegmentListPop(&cls->free);
}

static ChiHeapSegment* segmentNewChunk(ChiHeap* heap, ChiHeapClass* cls) {
    ChiChunk* c = chiHeapChunkNew(heap, cls->segmentWords * CHI_WORDSIZE, CHI_NEW_DEFAULT);

    size_t pageOff = CHI_CEIL(sizeof (ChiHeapSegment) + sizeof (void*) * cls->bins, sizeof (ChiHeapPage));
    ChiHeapSegment* s = (ChiHeapSegment*)chiZalloc(pageOff + cls->pagesPerSegment * sizeof (ChiHeapPage));
    chiHeapSegmentListPoison(s);
    s->chunk = c;
    s->cls = cls;
    s->firstPage = CHI_ALIGN_CAST((uint8_t*)s + pageOff, ChiHeapPage*);
    s->sweepIndex = s->endIndex = 0;
    return s;
}

static void segmentAcquireNew(ChiHeap* heap, ChiHeapClass* cls, ChiHeapSegmentHandle* sh) {
    CHI_ASSERT(!sh->segment);
    sh->segment = segmentNewChunk(heap, cls);
    bool limit;
    {
        CHI_LOCK_MUTEX(&heap->mutex);
        ++cls->total;
        limit = classLimitReached(cls);
    }
    if (limit)
        chiHeapLimitReached(heap);
}

static void segmentAcquire(ChiHeap* heap, ChiHeapClass* cls, ChiHeapSegmentHandle* sh) {
    {
        CHI_LOCK_MUTEX(&heap->mutex);
        segmentAcquireExisting(cls, sh);
        if (sh->segment)
            return;
    }
    segmentAcquireNew(heap, cls, sh);
}

static void segmentCollectStats(ChiHeapSegmentHandle* sh) {
    CHI_NOWARN_UNUSED(sh);
    chiCount(sh->segment->cls->allocWords, sh->allocWords);
    CHI_IF(CHI_COUNT_ENABLED, sh->allocWords = 0);
}

static void segmentDestroy(ChiHeapSegmentHandle* sh) {
    if (!sh->segment)
        return;
    segmentCollectStats(sh);
    chiHeapSegmentListAppend(&sh->segment->cls->free, sh->segment);
    sh->segment = 0;
}

static void segmentFull(ChiHeap* heap, ChiHeapSegmentHandle* sh) {
    CHI_ASSERT(sh->segment);
    bool limit;
    ChiHeapClass* cls = sh->segment->cls;

    {
        CHI_LOCK_MUTEX(&heap->mutex);
        CHI_ASSERT(sh->segment->chunk->size == CHI_WORDSIZE * cls->segmentWords);

        segmentCollectStats(sh);

        chiHeapSegmentListAppend(&cls->full, sh->segment);
        sh->segment = 0;
        limit = classLimitReached(cls);

        segmentAcquireExisting(cls, sh);
    }

    if (limit)
        chiHeapLimitReached(heap);

    if (!sh->segment)
        segmentAcquireNew(heap, cls, sh);
}

static ChiHeapPage* pageFindFree(ChiHeap* heap, ChiHeapSegmentHandle* sh) {
    for (;;) {
        ChiHeapSegment* s = sh->segment;
        ChiHeapClass* cls = s->cls;
        ChiHeapPage* p = s->freePageList;
        if (p) {
            s->freePageList = CHI_UN(HeapPage, *p) == UINT32_MAX ? 0 : s->firstPage + (CHI_UN(HeapPage, *p) & ~CHI_HEAP_PAGE_FREE);
        } else {
            if (s->endIndex == cls->pagesPerSegment) {
                segmentFull(heap, sh);
                continue;
            }
            p = s->firstPage + s->endIndex++;
            CHI_ASSERT(s->endIndex <= cls->pagesPerSegment);
        }
        return p;
    }
}

static void pageAlloc(ChiHeap* heap, ChiHeapSegmentHandle* sh, size_t size, uint32_t bin) {
    ChiHeapPage* p = pageFindFree(heap, sh);
    ChiHeapSegment* s = sh->segment;
    ChiHeapClass* cls = s->cls;
    ChiWord *ptr = (ChiWord*)s->chunk->start + cls->pageWords * (size_t)(p - s->firstPage),
            *end = ptr + cls->pageWords;
    uint32_t objectSize = CHI_UN(HeapPage, *p) = cls->linear
             ? (uint32_t)size + _CHI_OBJECT_HEADER_SIZE
             : 1U << chiLog2Ceil(size + _CHI_OBJECT_HEADER_SIZE);
    CHI_ASSERT(objectSize > _CHI_OBJECT_HEADER_SIZE);
    while (ptr + objectSize <= end) {
        ChiObject* obj = (ChiObject*)ptr;
        obj->free.next = s->freeObjectList[bin];
        s->freeObjectList[bin] = obj;
        CHI_ASSERT(!chiColorUsed(chiObjectColor(obj)));
        ptr += objectSize;
    }
}

static CHI_NOINL ChiObject* segmentAlloc(ChiHeap* heap, ChiHeapSegmentHandle* sh, size_t size, uint32_t bin) {
    pageAlloc(heap, sh, size, bin);
    ChiHeapSegment* s = sh->segment;
    ChiObject* obj = s->freeObjectList[bin];
    CHI_ASSERT(obj);
    s->freeObjectList[bin] = obj->free.next;
    return obj;
}

CHI_INL void* segmentNew(ChiHeap* heap, ChiHeapSegmentHandle* sh,
                         size_t size, ChiNewFlags flags, uint32_t bin, ChiColor allocColor) {
    ChiHeapSegment* s = sh->segment;
    CHI_ASSERT(s);
    CHI_ASSERT(!s->cls->linear || bin == (uint32_t)size - 1);
    CHI_ASSERT(s->cls->linear || bin == chiLog2Floor(size));
    CHI_ASSERT(bin < s->cls->bins);

    ChiObject* obj = s->freeObjectList[bin];
    if (CHI_LIKELY(obj))
        s->freeObjectList[bin] = obj->free.next;
    else
        obj = segmentAlloc(heap, sh, size, bin);

    objectInit(obj, size, allocColor, flags);
    chiCount(sh->allocWords, size + _CHI_OBJECT_HEADER_SIZE);
    return obj->payload;
}

static void* largeNew(ChiHeap* heap, size_t size, ChiNewFlags flags, ChiColor allocColor) {
    size_t chunkSizeW = CHI_ALIGNUP(size + _CHI_OBJECT_HEADER_SIZE, CHI_CHUNK_MIN_SIZE / CHI_WORDSIZE);

    ChiChunk* c = chiHeapChunkNew(heap, chunkSizeW * CHI_WORDSIZE, flags);
    if (CHI_UNLIKELY(!c))
        return 0;

    ChiObject* obj = (ChiObject*)c->start;
    objectInit(obj, size, allocColor, flags);
    bool limit;
    {
        CHI_LOCK_MUTEX(&heap->mutex);
        heap->large.totalWords += chunkSizeW;
        chiCount(heap->large.allocWords, chunkSizeW);
        chiChunkListAppend(&heap->large.list, c);
        heap->large.limitAlloc -= CHI_MIN(chunkSizeW, heap->large.limitAlloc);
        limit = !heap->large.limitAlloc;
    }
    if (limit)
        chiHeapLimitReached(heap);
    return obj->payload;
}

#define HEAP_NEW_CHECK(size, flags)                                     \
    ({                                                                  \
        CHI_ASSERT(size); /* no empty objects */                        \
        CHI_ASSERT(size <= UINT32_MAX); /* size overflow */             \
        CHI_ASSERT(flags & (CHI_NEW_LOCAL | CHI_NEW_SHARED)); /* either local or shared is required */ \
        CHI_ASSERT(!(flags & CHI_NEW_LOCAL) != !(flags & CHI_NEW_SHARED)); /* not both local and shared */ \
        CHI_ASSERT(!(flags & CHI_NEW_SHARED) || (flags & CHI_NEW_CLEAN)); \
    })

/**
 * Allocation of small objects on the major heap. This operation will never fail.
 */
void* chiHeapHandleNewSmall(ChiHeapHandle* handle, size_t size, ChiNewFlags flags) {
    HEAP_NEW_CHECK(size, flags);
    CHI_ASSERT(!(flags & CHI_NEW_TRY));
    CHI_ASSERT(size <= SMALL_BINS);
    return segmentNew(handle->heap, &handle->small, size, flags, (uint32_t)size - 1, handle->allocColor);
}

/**
 * Allocation of large objects on the major heap. This operation will may fail and
 * return 0, if the request cannot be fulfilled.
 */
void* chiHeapHandleNew(ChiHeapHandle* handle, size_t size, ChiNewFlags flags) {
    HEAP_NEW_CHECK(size, flags);
    ChiHeap* heap = handle->heap;
    if (CHI_LIKELY(size <= SMALL_BINS))
        return segmentNew(heap, &handle->small, size, flags, (uint32_t)size - 1, handle->allocColor);
    if (CHI_LIKELY(size < heap->medium.pageWords))
        return segmentNew(heap, &handle->medium, size, flags, chiLog2Floor(size), handle->allocColor);
    return largeNew(heap, size, flags, handle->allocColor);
}

void chiHeapSetup(ChiHeap* heap,
                  size_t smallChunkSize, size_t smallPageSize,
                  size_t mediumChunkSize, size_t mediumPageSize,
                  uint32_t limitFull, size_t limitInit) {
    CHI_ZERO_STRUCT(heap);
    chiMutexInit(&heap->mutex);

    heap->large.limitAllocPercent = limitFull / 2; // TODO better allocation limit
    heap->large.limitInit = heap->large.limitAlloc = limitInit / CHI_WORDSIZE;
    chiChunkListInit(&heap->large.list);

    classSetup(&heap->small, smallChunkSize, smallPageSize, limitFull, limitInit,
               SMALL_BINS, true);

    classSetup(&heap->medium, mediumChunkSize, mediumPageSize, limitFull, limitInit,
               chiLog2Floor(mediumPageSize - _CHI_OBJECT_HEADER_SIZE), false);
}

void chiHeapDestroy(ChiHeap* heap) {
    chiMutexDestroy(&heap->mutex);
    chiHeapChunkListFree(heap, &heap->large.list);
    classDestroy(heap, &heap->small);
    classDestroy(heap, &heap->medium);
    CHI_POISON_STRUCT(heap, CHI_POISON_DESTROYED);
}

void chiHeapHandleSetup(ChiHeapHandle* handle, ChiHeap* heap, ChiColor color) {
    CHI_ZERO_STRUCT(handle);
    handle->heap = heap;
    handle->allocColor = color;
    segmentAcquire(heap, &heap->small, &handle->small);
    segmentAcquire(heap, &heap->medium, &handle->medium);
}

void chiHeapHandleDestroy(ChiHeapHandle* handle) {
    ChiHeap* heap = handle->heap;
    CHI_LOCK_MUTEX(&heap->mutex);
    segmentDestroy(&handle->small);
    segmentDestroy(&handle->medium);
}

void chiHeapSweepReleaseSegment(ChiHeap* heap, ChiHeapSegment* s) {
    CHI_ASSERT(!s->sweepIndex);
    ChiHeapClass* cls = s->cls;
    if (s->alive) {
        CHI_LOCK_MUTEX(&heap->mutex);
        chiHeapSegmentListAppend(&cls->free, s);
    } else {
        {
            CHI_LOCK_MUTEX(&heap->mutex);
            CHI_ASSERT(cls->total > 0);
            --cls->total;
        }
        chiHeapChunkFree(heap, s->chunk);
        chiFree(s);
    }
}

void chiHeapSweepAcquire(ChiHeap* heap, ChiChunkList* largeList, ChiHeapSegmentList* segmentList) {
    CHI_LOCK_MUTEX(&heap->mutex);
    chiHeapSegmentListJoin(segmentList, &heap->small.full);
    chiHeapSegmentListJoin(segmentList, &heap->medium.full);
    chiChunkListJoin(largeList, &heap->large.list);
}

void chiHeapSweepReleaseLarge(ChiHeap* heap, ChiChunkList* list, size_t freed) {
    CHI_LOCK_MUTEX(&heap->mutex);
    CHI_ASSERT(heap->large.totalWords >= freed);
    heap->large.totalWords -= freed;
    heap->large.limitAlloc = CHI_MAX(heap->large.limitInit,
                                     heap->large.totalWords * heap->large.limitAllocPercent / 100);
    chiChunkListJoin(&heap->large.list, list);
}

const char* chiTypeName(ChiType type) {
    switch (type) {
    case CBY_INTERP_MODULE:            return "INTERP_MODULE";
    case CBY_NATIVE_MODULE:            return "NATIVE_MODULE";
    case CHI_ARRAY_SMALL:              return "ARRAY_SMALL";
    case CHI_ARRAY_LARGE:              return "ARRAY_LARGE";
    case CHI_STRINGBUILDER:            return "STRINGBUILDER";
    case CHI_PRESTRING:                return "PRESTRING";
    case CHI_THREAD:                   return "THREAD";
    case CHI_THUNK:                    return "THUNK";
    case CHI_STACK:                    return "STACK";
    case CHI_BIGINT_POS:
    case CHI_BIGINT_NEG:               return "BIGINT";
    case CBY_FFI:                      return "FFI";
    case CHI_BUFFER0...CHI_BUFFER7:    return "BUFFER";
    case CHI_STRING:                   return "STRING";
    case CHI_BOX:                      return "BOX";
    case CHI_FIRST_TAG...CHI_LAST_TAG: return "DATA";
    case CHI_FIRST_FN...CHI_LAST_FN:   return "FN";
    case CHI_FAIL:                     return "FAIL";
    case CHI_POISON_TAG:               return "POISON";
    default: CHI_BUG("Invalid type");
    }
}
