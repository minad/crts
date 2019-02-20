#include "runtime.h"
#include "num.h"
#include "chunk.h"
#include "mem.h"
#include "timeout.h"

enum {
    SWEEP_LARGE     = 1,
    SWEEP_LARGEACQ  = 2,
    SWEEP_SEGMENTS  = 4,
};

CHI_INL CHI_WU ChiHeapSegment* outerHeapSegment(ChiList* entry) {
    return CHI_OUTER(ChiHeapSegment, list, entry);
}

static void heapUsage(ChiHeap* heap, ChiHeapUsage* usage) {
    usage->small = heap->cls[0].usage;
    usage->medium = heap->cls[1].usage;
    usage->large = heap->large.usage;
}

void chiHeapUsage(ChiHeap* heap, ChiHeapUsage* usage) {
    CHI_LOCK(&heap->mutex);
    heapUsage(heap, usage);
}

CHI_INL void sweepCount(ChiObjectCount* count, size_t words) {
    if (CHI_STATS_SWEEP_ENABLED)
        chiCountObject(count, words);
}

static void checkAllocLimit(ChiHeap* heap, const ChiHeapUsage* usage) {
    size_t allocWords = usage->large.allocSinceSweep + usage->medium.allocSinceSweep + usage->small.allocSinceSweep,
        totalWords = usage->large.totalWords + usage->medium.totalWords + usage->small.totalWords;
    if (allocWords > ((uint64_t)heap->allocLimit * totalWords) / 100)
        chiHeapAllocLimitReached(heap);
}

CHI_INL size_t objectSize(const ChiObject* o) {
    return chiObjectSize(o) + _CHI_HEADER_OVERHEAD;
}

CHI_INL size_t binSize(uint32_t subShift, uint32_t bin) {
    size_t level = bin >> subShift;
    uint32_t sub = bin & ((1U << subShift) - 1);
    return (1UL << level) + (level < subShift ? 0 : sub << (level - subShift));
}

CHI_INL uint32_t _binIndex(uint32_t subShift, size_t fullSize) {
    uint32_t
        level = chiLog2(fullSize),
        sub   = level < subShift ? 0 : (uint32_t)((fullSize & ((1UL << level) - 1)) >> (level - subShift)),
        bin   = (level << subShift) + sub;
    CHI_ASSERT(sub < (1U << subShift));
    //chiWarn("subShift=%u fullSize=%zu bin=%u level=%u sub=%u binSize=%zu",
    //          subShift, fullSize, bin, level, sub, binSize(subShift, bin));
    return bin;
}

CHI_INL uint32_t binIndexSmaller(uint32_t subShift, size_t fullSize) {
    uint32_t bin = _binIndex(subShift, fullSize);
    CHI_ASSERT(_binIndex(subShift, binSize(subShift, bin)) == bin);
    CHI_ASSERT(binSize(subShift, bin) <= fullSize);
    return bin;
}

CHI_INL uint32_t binIndexLarger(uint32_t subShift, size_t fullSize) {
    uint32_t bin = _binIndex(subShift, fullSize);
    bin += binSize(subShift, bin) < fullSize;
    CHI_ASSERT(_binIndex(subShift, binSize(subShift, bin)) == bin);
    CHI_ASSERT(binSize(subShift, bin) >= fullSize);
    return bin;
}

static void classDestroy(ChiHeap* heap, ChiHeapClass* cls) {
    for (uint32_t i = 0; i < CHI_DIM(cls->izone); ++i) {
        while (!chiListNull(cls->izone + i)) {
            ChiHeapSegment* s = outerHeapSegment(chiListPop(cls->izone + i));
            chiHeapChunkFree(heap, s->chunk);
            chiFree(s);
        }
    }
}

static void classSetup(ChiHeapClass* cls, const ChiHeapDesc* d) {
    cls->desc = *d;
    for (uint32_t i = 0; i < CHI_DIM(cls->izone); ++i)
        chiListInit(cls->izone + i);
}

CHI_INL void segmentInsert(const ChiHeapDesc* d, ChiHeapSegment* s, ChiWord* p, size_t fullSize) {
    ChiObject* obj = CHI_ALIGN_CAST32(p, ChiObject*);
    chiObjectInit(obj, fullSize, CHI_WHITE);
    // Objects which are too small are not useable.
    // Too small objects have a size equal to header overhead
    // or belong to the bin of a smaller zone.
    // They will be merged by the sweeper
    // with adjacent objects as soon as those are freed.
    uint32_t bin = binIndexSmaller(d->sub, fullSize);
    if (bin >= d->off) {
        bin = CHI_MIN(bin - d->off, d->bins - 1);
        obj->next = s->bin[bin];
        s->bin[bin] = obj;
    }
}

CHI_INL void segmentInsertRest(const ChiHeapDesc* d, ChiHeapSegment* s, ChiObject* obj, size_t neededSize) {
    size_t objSize = objectSize(obj), restSize = objSize - neededSize;
    if (restSize) // is there some rest?
        segmentInsert(d, s, obj->payload + neededSize - _CHI_HEADER_OVERHEAD, restSize);
}

CHI_INL ChiObject* segmentFind(const ChiHeapDesc* d, ChiHeapSegment* s, size_t fullSize) {
    uint32_t bin = binIndexLarger(d->sub, fullSize);
    CHI_ASSERT(bin >= d->off);
    bin -= d->off;
    while (bin < d->bins) {
        ChiObject* obj = s->bin[bin];
        if (obj) {
            CHI_ASSERT(objectSize(obj) > binSize(d->sub, bin + d->off - 1));
            CHI_ASSERT(objectSize(obj) >= binSize(d->sub, bin + d->off));
            CHI_ASSERT(bin == d->bins - 1 || objectSize(obj) < binSize(d->sub, bin + d->off + 1));
            s->bin[bin] = obj->next;
            return obj;
        }
        ++bin;
    }
    return 0;
}

static ChiHeapSegment* segmentPop(ChiHeapClass* cls, ChiList* zone) {
    ChiHeapSegment* s = outerHeapSegment(chiListPop(zone));
    chiListAppend(&cls->zone.used, &s->list);
    return s;
}

static void segmentPush(ChiList* zone, ChiHeapSegment* s) {
    chiListDelete(&s->list);
    chiListAppend(zone, &s->list);
}

CHI_INL ChiHeapSegment* segmentNew(ChiHeap* heap, const ChiHeapDesc* d) {
    ChiChunk* r = chiHeapChunkNew(heap, d->chunk * CHI_WORDSIZE, CHI_HEAP_NOFAIL);
    ChiHeapSegment* s = (ChiHeapSegment*)chiZalloc(sizeof (ChiHeapSegment) + sizeof (void*) * d->bins);
    s->chunk = r;
    chiListPoison(&s->list);
    segmentInsert(d, s, (ChiWord*)s->chunk->start, d->chunk);
    return s;
}

static void segmentRelease(ChiHeapClass* cls, ChiHeapUsedSegment* us) {
    CHI_ASSERT(us->segment->chunk->size == CHI_WORDSIZE * cls->desc.chunk);
    segmentPush(us->zone, us->segment);
    cls->usage.allocSinceStart += us->allocWords;
    cls->usage.allocSinceSweep += us->allocWords;
    us->segment = 0;
    us->allocWords = 0;
}

static void segmentReleaseFull(ChiHeap* heap, ChiHeapClass* cls, ChiHeapUsedSegment* us) {
    bool notify;
    ChiHeapUsage usage;
    {
        CHI_LOCK(&heap->mutex);
        CHI_ASSERT(us->segment->chunk->size == CHI_WORDSIZE * cls->desc.chunk);
        // Move segment from free to full zone which comes after the free zone (zone + 1)
        segmentPush(us->zone + 1, us->segment);
        cls->usage.allocSinceStart += us->allocWords;
        cls->usage.allocSinceSweep += us->allocWords;
        notify = heap->sweep != 0;
        heapUsage(heap, &usage);
    }
    us->allocWords = 0;
    us->segment = 0;
    if (notify)
        chiHeapSweepNotify(heap);
    checkAllocLimit(heap, &usage);
}

// Select zone for new allocations.
// Set color to black if allocating in not-yet swept zone.
static void segmentAcquire(ChiHeap* heap, ChiHeapClass* cls, ChiHeapUsedSegment* us) {
    ChiHeapUsage usage;
    {
        CHI_LOCK(&heap->mutex);

        if (!chiListNull(&cls->zone.sweptFree)) {
            // Use segment from free already swept zone (happens only in sweeping phase)
            us->zone = &cls->zone.sweptFree;
            us->color = CHI_WHITE;
            us->segment = segmentPop(cls, us->zone);
            return;
        }

        if (!chiListNull(&cls->zone.free)) {
            // Use segment from free zone (not yet swept!)
            // mark black if sweeping
            us->zone = &cls->zone.free;
            us->color = heap->sweep & SWEEP_SEGMENTS ? CHI_BLACK : CHI_WHITE;
            us->segment = segmentPop(cls, us->zone);
            return;
        }

        // create new segment (free not-yet swept or free already swept, depending on heap->sweep)
        cls->usage.totalWords += cls->desc.chunk;
        us->zone = heap->sweep & SWEEP_SEGMENTS ? &cls->zone.sweptFree : &cls->zone.free;
        heapUsage(heap, &usage);
    }

    checkAllocLimit(heap, &usage);
    us->color = CHI_WHITE;
    us->segment = segmentNew(heap, &cls->desc);

    {
        CHI_LOCK(&heap->mutex);
        chiListAppend(&cls->zone.used, &us->segment->list);
    }
}

CHI_INL Chili smallNew(ChiHeap* heap, ChiHeapClass* cls, ChiHeapUsedSegment* us,
                       ChiType type, size_t fullSize, ChiColor color) {
    ChiObject* obj;
    for (;;) {
        if (CHI_UNLIKELY(!us->segment))
            segmentAcquire(heap, cls, us);
        obj = segmentFind(&cls->desc, us->segment, fullSize);
        if (CHI_LIKELY(obj))
            break;
        segmentReleaseFull(heap, cls, us);
    }
    segmentInsertRest(&cls->desc, us->segment, obj, fullSize);
    chiObjectInit(obj, fullSize, (ChiColor)(us->color | color));
    us->allocWords += fullSize;
    return chiWrap(obj->payload, _CHILI_PINNED, type);
}

static void segmentSweep(ChiHeapClass* cls, ChiHeapSegment* s, ChiSweepClassStats* stats, ChiTimeout* timeout) {
    CHI_ASSERT(s->chunk->size == CHI_WORDSIZE * cls->desc.chunk);
    ChiWord* p = s->sweep, *end = (ChiWord*)s->chunk->start + cls->desc.chunk;
    bool live = false;
    while (p < end && chiTimeoutTick(timeout)) {
        ChiWord* free = p;
        while (p < end) {
            ChiObject* obj = CHI_ALIGN_CAST32(p, ChiObject*);
            if (chiObjectColor(obj) == CHI_BLACK)
                break;
            CHI_ASSERT(chiObjectColor(obj) == CHI_WHITE);
            p += objectSize(obj);
        }
        if (free != p) {
            size_t size = (size_t)(p - free);
            sweepCount(&stats->free, size);
            segmentInsert(&cls->desc, s, free, size);
        }
        CHI_ASSERT(p <= end);
        if (p == end)
            break;
        ChiObject* obj = CHI_ALIGN_CAST32(p, ChiObject*);
        chiObjectSetColor(obj, CHI_WHITE);
        size_t size = objectSize(obj);
        sweepCount(&stats->live, size);
        p += size;
        live = true;
    }
    if (live)
        s->live = true;
    s->sweep = p;
}

static Chili largeNew(ChiHeap* heap, ChiType type, size_t fullSize, ChiColor color, ChiHeapFail fail) {
    size_t chunkSizeW = CHI_ROUNDUP(fullSize, CHI_CHUNK_MIN_SIZE / CHI_WORDSIZE);

    ChiChunk* r = chiHeapChunkNew(heap, chunkSizeW * CHI_WORDSIZE, fail);
    if (CHI_UNLIKELY(!r))
        return CHI_FAIL;

    ChiObject* obj = (ChiObject*)r->start;
    chiObjectInit(obj, fullSize, color);
    ChiHeapUsage usage;
    {
        CHI_LOCK(&heap->mutex);
        heap->large.usage.totalWords += chunkSizeW;
        heap->large.usage.allocSinceStart += chunkSizeW;
        heap->large.usage.allocSinceSweep += chunkSizeW;
        chiListAppend(heap->sweep & SWEEP_LARGE ? &heap->large.listSwept : &heap->large.list, &r->list);
        heapUsage(heap, &usage);
    }
    checkAllocLimit(heap, &usage);
    return chiWrap(obj->payload, _CHILI_PINNED, type);
}

static void largeSweepAcquire(ChiHeap* heap, ChiList* largeChunks) {
    // We remove the unswept large chunks from the heap list
    // Unfortunately the chunks will not appear in heap dumps then.
    if (heap->sweep & SWEEP_LARGE) {
        heap->sweep &= ~SWEEP_LARGE;
        if (!chiListNull(&heap->large.list)) {
            heap->sweep |= SWEEP_LARGEACQ;
            chiListJoin(largeChunks, &heap->large.list);
        }
        chiListJoin(&heap->large.list, &heap->large.listSwept);
    }
}

static void largeSweep(ChiHeap* heap, ChiSweepClassStats* stats, ChiList* largeChunks) {
    size_t freedBytes = 0;
    CHI_FOREACH_CHUNK (r, largeChunks) {
        ChiObject* obj = (ChiObject*)r->start;
        if (chiObjectColor(obj) == CHI_BLACK) {
            size_t size = objectSize(obj);
            sweepCount(&stats->live, size);
            chiObjectSetColor(obj, CHI_WHITE);
        } else {
            freedBytes += r->size;
            sweepCount(&stats->free, objectSize(obj));
            chiListDelete(&r->list);
            chiHeapChunkFree(heap, r);
        }
    }

    {
        CHI_LOCK(&heap->mutex);
        CHI_ASSERT(heap->large.usage.totalWords >= freedBytes / CHI_WORDSIZE);
        heap->large.usage.totalWords -= freedBytes / CHI_WORDSIZE;
        heap->sweep &= ~SWEEP_LARGEACQ;
        chiListJoin(&heap->large.list, largeChunks);
    }
}

// make swept zones normal zones again
static void classReset(ChiHeapClass* cls) {
    CHI_ASSERT(chiListNull(&cls->zone.free));
    CHI_ASSERT(chiListNull(&cls->zone.full));
    CHI_ASSERT(chiListNull(&cls->zone.sweep));
    chiListJoin(&cls->zone.free, &cls->zone.sweptFree);
    chiListJoin(&cls->zone.full, &cls->zone.sweptFull);
}

static bool segmentsUsed(ChiHeap* heap) {
    for (uint32_t i = 0; i < CHI_DIM(heap->cls); ++i) {
        if (!chiListNull(&heap->cls[i].zone.used))
            return true;
    }
    return false;
}

static void segmentSweepFinalize(ChiHeap* heap) {
    CHI_ASSERT(heap->sweep & SWEEP_SEGMENTS);
    CHI_ASSERT(!segmentsUsed(heap));
    for (uint32_t i = 0; i < CHI_DIM(heap->cls); ++i)
        classReset(heap->cls + i);
    heap->sweep &= ~SWEEP_SEGMENTS;
}

static ChiHeapSegment* segmentSweepAcquire(ChiHeap* heap, uint32_t* cls) {
    if (!(heap->sweep & SWEEP_SEGMENTS))
        return 0;

    for (uint32_t i = 0; i < CHI_DIM(heap->cls); ++i) {
        if (!chiListNull(&heap->cls[i].zone.sweep)) {
            *cls = i;
            return segmentPop(heap->cls + i, &heap->cls[i].zone.sweep);
        }
    }

    for (uint32_t i = 0; i < CHI_DIM(heap->cls); ++i) {
        if (!chiListNull(&heap->cls[i].zone.full)) {
            *cls = i;
            ChiHeapSegment* s = segmentPop(heap->cls + i, &heap->cls[i].zone.full);
            memset(s->bin, 0, sizeof (void*) * heap->cls[i].desc.bins);
            s->sweep = (ChiWord*)s->chunk->start;
            s->live = false;
            return s;
        }
    }

    for (uint32_t i = 0; i < CHI_DIM(heap->cls); ++i) {
        if (!chiListNull(&heap->cls[i].zone.free)) {
            *cls = i;
            ChiHeapSegment* s = segmentPop(heap->cls + i, &heap->cls[i].zone.free);
            memset(s->bin, 0, sizeof (void*) * heap->cls[i].desc.bins);
            s->sweep = (ChiWord*)s->chunk->start;
            s->live = false;
            return s;
        }
    }

    if (!segmentsUsed(heap))
        segmentSweepFinalize(heap);

    return 0;
}

static void segmentSweepRelease(ChiHeap* heap, ChiHeapClass* cls, ChiHeapSegment* s) {
    CHI_ASSERT(s->chunk->size == CHI_WORDSIZE * cls->desc.chunk);

    if (s->live) {
        CHI_LOCK(&heap->mutex);
        CHI_ASSERT(heap->sweep & SWEEP_SEGMENTS);
        ChiWord* end = (ChiWord*)s->chunk->start + cls->desc.chunk;
        segmentPush(end == s->sweep ? &cls->zone.sweptFree : &cls->zone.sweep, s);
    } else {
        {
            CHI_LOCK(&heap->mutex);
            CHI_ASSERT(heap->sweep & SWEEP_SEGMENTS);
            CHI_ASSERT(cls->usage.totalWords >= cls->desc.chunk);
            chiListDelete(&s->list);
            cls->usage.totalWords -= cls->desc.chunk;
        }
        chiHeapChunkFree(heap, s->chunk);
        chiFree(s);
    }
}

void chiSweepBegin(ChiHeap* heap) {
    CHI_LOCK(&heap->mutex);
    CHI_ASSERT(!heap->sweep);
    CHI_ASSERT(!segmentsUsed(heap));
    heap->large.usage.allocSinceSweep = 0;
    for (size_t i = 0; i < CHI_DIM(heap->cls); ++i)
        heap->cls[i].usage.allocSinceSweep = 0;
    heap->sweep = SWEEP_SEGMENTS | SWEEP_LARGE;
}

bool chiSweepEnd(ChiHeap* heap) {
    CHI_LOCK(&heap->mutex);
    return !heap->sweep;
}

void chiSweepSlice(ChiHeap* heap, ChiSweepStats* pstats, ChiTimeout* timeout) {
    ChiSweepClassStats stats[CHI_DIM(heap->cls) + 1];
    CHI_CLEAR_ARRAY(stats);

    while (chiTimeoutTick(timeout)) {
        ChiList largeChunks = CHI_LIST_INIT(largeChunks);
        uint32_t cls;
        ChiHeapSegment* s;
        {
            CHI_LOCK(&heap->mutex);
            s = segmentSweepAcquire(heap, &cls);
            if (!s) {
                largeSweepAcquire(heap, &largeChunks);
                if (chiListNull(&largeChunks))
                    break;
            }
        }

        if (s) {
            segmentSweep(heap->cls + cls, s, stats + cls, timeout);
            segmentSweepRelease(heap, heap->cls + cls, s);
        }

        if (!chiListNull(&largeChunks))
            largeSweep(heap, stats + CHI_DIM(stats) - 1, &largeChunks);
    }

    *pstats = (ChiSweepStats){ stats[0], stats[1], stats[2] };
}

void chiHeapSetup(ChiHeap* heap, const ChiHeapDesc* small, const ChiHeapDesc* medium,
                  uint32_t allocLimit) {
    CHI_CLEAR(heap);
    heap->allocLimit = allocLimit;
    chiListInit(&heap->large.list);
    chiListInit(&heap->large.listSwept);
    chiMutexInit(&heap->mutex);

    ChiHeapDesc s = *small, m = *medium;
    s.off = 1;
    s.bins = binIndexSmaller(s.sub, s.max) + 1 - s.off;
    m.off = binIndexSmaller(m.sub, s.max);
    m.bins = binIndexSmaller(m.sub, m.max) + 1 - m.off;

    classSetup(&heap->cls[0], &s);
    classSetup(&heap->cls[1], &m);
}

static void heapChunkFreeList(ChiHeap* heap, ChiList* list) {
    while (!chiListNull(list))
        chiHeapChunkFree(heap, chiChunkPop(list));
}

void chiHeapDestroy(ChiHeap* heap) {
    CHI_ASSERT(!segmentsUsed(heap));
    chiMutexDestroy(&heap->mutex);
    heapChunkFreeList(heap, &heap->large.list);
    heapChunkFreeList(heap, &heap->large.listSwept);
    for (uint32_t i = 0; i < CHI_DIM(heap->cls); ++i)
        classDestroy(heap, heap->cls + i);
    chiPoisonMem(heap, CHI_POISON_DESTROYED, sizeof (ChiHeap));
}

/**
 * Allocation of small objects on the major heap (< 1K). This operation will never fail.
 */
Chili chiHeapNewSmall(ChiHeapHandle* handle, ChiType type, size_t size, ChiColor color) {
    ChiHeap* heap = handle->heap;
    size_t fullSize = size + _CHI_HEADER_OVERHEAD;
    CHI_ASSERT(size && fullSize < heap->cls[0].desc.max);
    return smallNew(heap, heap->cls, handle->usedSegment, type, fullSize, color);
}

/**
 * Allocation of large objects on the major heap. This operation will may fail and
 * return CHI_FAIL, if the request cannot be fulfilled.
 */
Chili chiHeapNewLarge(ChiHeapHandle* handle, ChiType type, size_t size, ChiColor color, ChiHeapFail fail) {
    size_t fullSize = size + _CHI_HEADER_OVERHEAD;
    CHI_ASSERT(size && fullSize <= CHI_CHUNK_MAX_SIZE / CHI_WORDSIZE);
    ChiHeap* heap = handle->heap;
    if (CHI_LIKELY(fullSize < handle->heap->cls[1].desc.max)) {
        uint32_t clsIdx = fullSize >= heap->cls[0].desc.max;
        return smallNew(heap, heap->cls + clsIdx, handle->usedSegment + clsIdx, type, fullSize, color);
    }
    return largeNew(heap, type, fullSize, color, fail);
}

bool chiHeapHandleReleaseUnlocked(ChiHeapHandle* handle) {
    ChiHeap* heap = handle->heap;
    bool notify = false;
    for (uint32_t i = 0; i < CHI_DIM(handle->usedSegment); ++i) {
        if (handle->usedSegment[i].segment) {
            segmentRelease(heap->cls + i, handle->usedSegment + i);
            notify = true;
        }
    }
    return notify;
}

void chiHeapHandleRelease(ChiHeapHandle* handle) {
    ChiHeap* heap = handle->heap;
    bool notify = false;
    {
        CHI_LOCK(&heap->mutex);
        for (uint32_t i = 0; i < CHI_DIM(handle->usedSegment); ++i) {
            if (handle->usedSegment[i].segment)
                notify = chiHeapHandleReleaseUnlocked(handle) || notify;
        }
        notify = notify && heap->sweep;
    }
    if (notify)
        chiHeapSweepNotify(heap);
}

const char* chiTypeName(ChiType type) {
    switch (type) {
    case CBY_INTERP_MODULE: return "INTERP_MODULE";
    case CBY_NATIVE_MODULE: return "NATIVE_MODULE";
    case CHI_ARRAY:         return "ARRAY";
    case CHI_THUNKFN:       return "THUNKFN";
    case CHI_STRINGBUILDER: return "STRINGBUILDER";
    case CHI_RAW:           return "RAW";
    case CHI_THREAD:        return "THREAD";
    case CHI_THUNK:         return "THUNK";
    case CHI_STACK:         return "STACK";
    case CHI_BIGINT:        return "BIGINT";
    case CBY_FFI:           return "FFI";
    case CHI_BUFFER:        return "BUFFER";
    case CHI_STRING:        return "STRING";
    case CHI_BOX64:         return "BOX64";
    case CHI_FIRST_TAG...CHI_LAST_TAG: return "DATA";
    case CHI_FIRST_FN...CHI_LAST_FN: return "FN";
    case CHI_FAIL_OBJECT:   return "FAIL";
    case CHI_POISON_OBJECT: return "POISON";
    default: CHI_BUG("Invalid type");
    }
}
