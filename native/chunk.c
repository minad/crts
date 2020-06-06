#include "chunk.h"
#include "error.h"
#include "num.h"
#include "pool.h"
#include "rtoption.h"
#include "system.h"

#if CHI_HEAPTRACK_ENABLED
#  include <heaptrack_api.h>
#else
#  define heaptrack_report_alloc(x, y) ({})
#  define heaptrack_report_free(x)     ({})
#endif

#if CHI_CHUNK_ARENA_ENABLED
#  define LIST        AddrList
#  define LIST_LINK   ChiChunkAddrLink
#  define LIST_PREFIX addrList
#  define LIST_ELEM   ChiChunk
#  define LIST_LINK_FIELD addrLink
#  include "generic/list.h"
#endif

typedef struct CHI_ALIGNED(8) Allocator_ {
    ChiChunk* (*newChunk)(struct Allocator_*, size_t, size_t);
    void (*freeChunk)(struct Allocator_*, ChiChunk*);
} Allocator;

typedef struct {
    Allocator   a;
    CHI_IF(CHI_CHUNK_ARENA_ENABLED,
           ChiVirtMem   mem;
           ChiChunkList sizeList[CHI_CHUNK_MAX + 1];
           AddrList     addrList;
           ChiChunk     arena;)
    ChiMutex    mutex;
    ChiPool     handlePool;
    bool        smallPages;
} VirtAllocator;

#if CHI_UNMAP_TASK_ENABLED
typedef struct {
    VirtAllocator va;
    ChiCond       cond;
    ChiChunkList  list;
    ChiTask       task;
    ChiMillis     timeout;
} UnmapAllocator;
static UnmapAllocator _allocator;
#define ALLOCATOR (&_allocator.va.a)
#else
static VirtAllocator _allocator;
#define ALLOCATOR (&_allocator.a)
#endif

CHI_LOCAL bool chunkAlignValid(size_t align) {
    return !align || (chiIsPow2(align)
                      && align >= CHI_CHUNK_MIN_SIZE
                      && align <= CHI_CHUNK_MAX_SIZE);
}

static bool chunkAligned(void* start, size_t align) {
    if (!align)
        return true;
    CHI_ASSERT(chunkAlignValid(align));
    return !((uintptr_t)start & (align - 1));
}

static void chunkInit(ChiChunk* chunk, void* start, size_t size) {
    CHI_IF(CHI_CHUNK_ARENA_ENABLED, chunk->_used = false; addrListPoison(chunk));
    chiChunkListPoison(chunk);
    chunk->size = size;
    chunk->start = start;
    CHI_IF(CHI_UNMAP_TASK_ENABLED, chunk->_unmapTimeout = chiMillis(0));
}

static ChiChunk* vaGetHandle(VirtAllocator*, void*, size_t);
static void vaFreeChunk(Allocator*, ChiChunk*);
static CHI_WU bool vaNeedHandles(VirtAllocator*, size_t);
static CHI_WU bool vaAllocHuge(VirtAllocator*, size_t);

_Static_assert(CHI_CHUNK_ARENA_ENABLED || CHI_SYSTEM_HAS_VIRTALLOC, "System does not use unfixed virtual allocation.");
_Static_assert(CHI_CHUNK_ARENA_ENABLED || !CHI_CHUNK_START, "If chunk arena is disabled, chunks start at 0.");
_Static_assert(CHI_CHUNK_ARENA_ENABLED || !CHI_CHUNK_ARENA_FIXED, "Fixed arena requires enabled arena.");
_Static_assert(CHI_CHUNK_ARENA_FIXED || !CHI_CHUNK_START, "Fixed arena must not start at 0.");

#if CHI_CHUNK_ARENA_ENABLED

CHI_INL CHI_WU uint32_t chunkClassSmaller(size_t size) {
    uint32_t cls = chiLog2Floor(size);
    CHI_ASSERT(cls >= CHI_CHUNK_MIN_SHIFT);
    CHI_ASSERT(cls <= CHI_CHUNK_MAX_SHIFT);
    return (uint32_t)(cls - CHI_CHUNK_MIN_SHIFT);
}

CHI_INL CHI_WU uint32_t chunkClassLarger(size_t size) {
    uint32_t cls = chunkClassSmaller(size);
    return ((size_t)1 << (cls + CHI_CHUNK_MIN_SHIFT)) < size ? cls + 1 : cls;
}

static void vaCoalesce(VirtAllocator* va, ChiChunk* chunk, ChiChunk* s) {
    CHI_ASSERT((uint8_t*)chunk->start + chunk->size == s->start);
    addrListDelete(s);
    chunk->size += s->size;
    chiPoolPut(&va->handlePool, s);
}

static void vaInsertFree(VirtAllocator* va, ChiChunk* chunk) {
    CHI_ASSERT(chunk->_used);
    chunk->_used = false;

    ChiChunk *before = addrListPrev(&va->addrList, chunk);
    if (before && !before->_used) {
        chiChunkListDelete(before);
        vaCoalesce(va, before, chunk);
        chunk = before;
    }

    ChiChunk *after = addrListNext(&va->addrList, chunk);
    if (after && !after->_used) {
        chiChunkListDelete(after);
        vaCoalesce(va, chunk, after);
    }

    chiChunkListAppend(va->sizeList + chunkClassSmaller(chunk->size), chunk);
}

static void vaFreeChunk(Allocator* a, ChiChunk* chunk) {
    VirtAllocator* va = (VirtAllocator*)a;
    CHI_ASSERT(chunk->_used);
    chiVirtDecommit(&va->mem, chunk->start, chunk->size);
    CHI_LOCK_MUTEX(&va->mutex);
    vaInsertFree(va, chunk);
}

static void vaSetupArena(VirtAllocator* va, uintptr_t arenaStart, size_t arenaSize) {
    CHI_ASSERT(chunkClassSmaller(CHI_CHUNK_MAX_SIZE) == CHI_CHUNK_MAX);
    CHI_ASSERT(arenaSize >= CHI_CHUNK_MIN_SIZE);

    if (!chiVirtReserve(&va->mem, (void*)arenaStart, arenaSize))
	chiSysErr("chiVirtReserve");

    addrListInit(&va->addrList);
    for (uint32_t i = 0; i <= CHI_CHUNK_MAX; ++i)
        chiChunkListInit(va->sizeList + i);

    chunkInit(&va->arena, (void*)arenaStart, arenaSize);
    chiChunkListAppend(va->sizeList + chunkClassSmaller(arenaSize), &va->arena);
    addrListAppend(&va->addrList, &va->arena);
}

static ChiChunk* vaFindSize(VirtAllocator* va, const size_t size) {
    uint32_t cls = chunkClassLarger(size);
    while (cls <= CHI_CHUNK_MAX && chiChunkListNull(va->sizeList + cls))
        ++cls;
    if (cls == CHI_CHUNK_MAX + 1) {
        CHI_IF(CHI_SYSTEM_HAS_ERRNO, errno = ENOMEM);
        return 0;
    }
    ChiChunk* chunk = chiChunkListPop(va->sizeList + cls);
    CHI_ASSERT(chunk->size >= size);
    CHI_ASSERT(chunkClassSmaller(chunk->size) == cls);
    CHI_ASSERT(!chunk->_used);
    chunk->_used = true;
    return chunk;
}

static ChiChunk* vaSplitBegin(VirtAllocator* va, ChiChunk* chunk, size_t align) {
    if (!chunkAligned(chunk->start, align)) {
        uintptr_t oldStart = (uintptr_t)chunk->start, newStart = CHI_ALIGNUP(oldStart, align);
        ChiChunk* before = vaGetHandle(va, chunk->start, newStart - oldStart);
        chunk->start = (void*)newStart;
        CHI_ASSERT(chunk->size > before->size);
        chunk->size -= before->size;
        CHI_IF(CHI_CHUNK_ARENA_ENABLED, before->_used = true; addrListInsertBefore(chunk, before));
        return before;
    }
    return 0;
}

static ChiChunk* vaSplitEnd(VirtAllocator* va, ChiChunk* chunk, size_t size) {
    if (chunk->size != size) {
        CHI_ASSERT(chunk->size > size);
        ChiChunk* after = vaGetHandle(va, (void*)((uintptr_t)chunk->start + size), chunk->size - size);
        CHI_ASSERT(chunk->size > after->size);
        chunk->size -= after->size;
        CHI_IF(CHI_CHUNK_ARENA_ENABLED, after->_used = true; addrListInsertAfter(chunk, after));
        return after;
    }
    return 0;
}

static void vaSplit(VirtAllocator* va, ChiChunk* chunk, size_t size, size_t align,
                    ChiChunk** before, ChiChunk** after) {
    *before = vaSplitBegin(va, chunk, align);
    *after = vaSplitEnd(va, chunk, size);
}

static ChiChunk* vaFind(VirtAllocator* va, size_t size, size_t align) {
    CHI_LOCK_MUTEX(&va->mutex);
    if (!vaNeedHandles(va, 2))
        return 0;
    ChiChunk *chunk = vaFindSize(va, size + align);
    if (!chunk)
        return 0;
    ChiChunk *before, *after;
    vaSplit(va, chunk, size, align, &before, &after);
    if (before)
        vaInsertFree(va, before);
    if (after)
        vaInsertFree(va, after);
    return chunk;
}

static ChiChunk* vaNewChunk(Allocator* a, const size_t size, const size_t align) {
    VirtAllocator* va = (VirtAllocator*)a;
    CHI_ASSERT(chiChunkSizeValid(size));
    CHI_ASSERT(chunkAlignValid(align));
    CHI_ASSERT(size >= align);

    ChiChunk* chunk = vaFind(va, size, align);
    if (chunk && !chiVirtCommit(&va->mem, chunk->start, chunk->size, vaAllocHuge(va, chunk->size))) {
        CHI_LOCK_MUTEX(&va->mutex);
        vaInsertFree(va, chunk);
        return 0;
    }

    return chunk;
}

#else

static void vaFreeChunk(Allocator* a, ChiChunk* chunk) {
    VirtAllocator* va = (VirtAllocator*)a;
    chiVirtFree(chunk->start, chunk->size);
    CHI_LOCK_MUTEX(&va->mutex);
    chiPoolPut(&va->handlePool, chunk);
}

static ChiChunk* vaNewChunk(Allocator* a, const size_t size, const size_t align) {
    VirtAllocator* va = (VirtAllocator*)a;
    CHI_ASSERT(chiChunkSizeValid(size));
    CHI_ASSERT(chunkAlignValid(align));
    CHI_ASSERT(size >= align);

    for (uint32_t tries = 0; tries < CHI_CHUNK_VIRTALLOC_TRIES; ++tries) {
        uintptr_t start = (uintptr_t)chiVirtAlloc(0, size + align, vaAllocHuge(va, size));
        if (!start)
            return 0;
        if (align) {
            uintptr_t alignedStart = CHI_ALIGNUP(start, align);
            if (CHI_SYSTEM_HAS_PARTIAL_VIRTFREE) {
                if (alignedStart > start)
                    chiVirtFree((void*)start, alignedStart - start);
                uintptr_t end = start + size + align, alignedEnd = alignedStart + size;
                if (alignedEnd < end)
                    chiVirtFree((void*)alignedEnd, end - alignedEnd);
            } else {
                chiVirtFree((void*)start, size + align);
                start = (uintptr_t)chiVirtAlloc((void*)alignedStart, size, vaAllocHuge(va, size));
                if (start != alignedStart) {
                    if (start)
                        chiVirtFree((void*)start, size);
                    continue;
                }
            }
        }
        CHI_LOCK_MUTEX(&va->mutex);
        if (!vaNeedHandles(va, 1))
            return 0;
        return vaGetHandle(va, (void*)start, size);
    }
    return 0;
}
#endif

#if CHI_CHUNK_ARENA_ENABLED && !CHI_SYSTEM_HAS_VIRTALLOC
static bool vaReserveHandles(VirtAllocator* va) {
    ChiChunk* chunk = vaFindSize(va, CHI_CHUNK_MIN_SIZE);
    if (!chunk)
        return false;

    if (!chiVirtCommit(&va->mem, chunk->start, CHI_CHUNK_MIN_SIZE, 0)) {
        vaInsertFree(va, chunk);
        return false;
    }

    chiPoolGrow(&va->handlePool, chunk->start, CHI_CHUNK_MIN_SIZE);
    ChiChunk *after = vaSplitEnd(va, chunk, CHI_CHUNK_MIN_SIZE);
    if (after)
        vaInsertFree(va, after);

    return true;
}
#else
static bool vaReserveHandles(VirtAllocator* va) {
    void* start = chiVirtAlloc(0, CHI_CHUNK_MIN_SIZE, 0);
    if (!start)
        return false;
    chiPoolGrow(&va->handlePool, start, CHI_CHUNK_MIN_SIZE);
    return true;
}
#endif

static bool vaAllocHuge(VirtAllocator* va, size_t size) {
    return size >= CHI_MiB(2) && !va->smallPages;
}

static bool vaNeedHandles(VirtAllocator* va, size_t needed) {
    return chiPoolAvail(&va->handlePool, needed) || vaReserveHandles(va);
}

static ChiChunk* vaGetHandle(VirtAllocator* va, void* start, size_t size) {
    ChiChunk* chunk = (ChiChunk*)chiPoolGet(&va->handlePool);
    chunkInit(chunk, start, size);
    return chunk;
}

static void vaSetup(VirtAllocator* va, const ChiChunkOption* opts) {
    CHI_ZERO_STRUCT(va);
    chiMutexInit(&va->mutex);
    CHI_LOCK_MUTEX(&va->mutex);
    va->smallPages = opts->smallPages;
    chiPoolInit(&va->handlePool, sizeof (ChiChunk));
    CHI_IF(CHI_CHUNK_ARENA_ENABLED, vaSetupArena(va, opts->arenaStart, opts->arenaSize));
    va->a.newChunk = vaNewChunk;
    va->a.freeChunk = vaFreeChunk;
}

#if CHI_UNMAP_TASK_ENABLED
// Try to reuse recently freed chunk to reduce pressure on the kernel
static ChiChunk* uaReuse(UnmapAllocator* ua, size_t size, size_t align) {
    CHI_LOCK_MUTEX(&ua->va.mutex);
    CHI_FOREACH_CHUNK_NODELETE(chunk, &ua->list) {
        if (chunk->size == size && chunkAligned(chunk->start, align)) {
            chiChunkListDelete(chunk);
            return chunk;
        }
    }
    return 0;
}

static ChiMillis currentMillis(void) {
    return chiNanosToMillis(chiClockFast());
}

static ChiChunk* uaPopWait(UnmapAllocator* ua) {
    CHI_LOCK_MUTEX(&ua->va.mutex);
    for (;;) {
        ChiChunk* chunk = chiChunkListHead(&ua->list);
        if (chunk && chiMillisLess(chunk->_unmapTimeout, currentMillis())) {
            chiChunkListDelete(chunk);
            return chunk;
        }
        chiCondWait(&ua->cond, &ua->va.mutex);
    }
}

static ChiChunk* uaPopNowait(UnmapAllocator* ua) {
    CHI_LOCK_MUTEX(&ua->va.mutex);
    return chiChunkListNull(&ua->list) ? 0 : chiChunkListPop(&ua->list);
}

CHI_TASK(uaRun, arg) {
    UnmapAllocator* ua = (UnmapAllocator*)arg;
    chiTaskName("unmap");
    ChiChunk* chunk;
    while ((chunk = uaPopWait(ua)))
        vaFreeChunk(&ua->va.a, chunk);
    return 0;
}

static bool uaFreeEmergency(UnmapAllocator* ua) {
    ChiChunk* chunk;
    while ((chunk = uaPopNowait(ua)))
        vaFreeChunk(&ua->va.a, chunk);
    return !!chunk;
}

static bool uaFreeDelayed(UnmapAllocator* ua, ChiChunk* chunk) {
    ChiMillis time = currentMillis();
    chunk->_unmapTimeout = chiMillisAdd(time, ua->timeout);
    CHI_LOCK_MUTEX(&ua->va.mutex);
    chiChunkListAppend(&ua->list, chunk);
    ChiChunk* head = chiChunkListHead(&ua->list);
    if (head && chiMillisLess(head->_unmapTimeout, time)) {
        if (chiTaskNull(ua->task)) {
            ua->task = chiTaskTryCreate(uaRun, ua);
            if (chiTaskNull(ua->task))
                return false;
        }
        chiCondSignal(&ua->cond);
    }
    return true;
}

static void uaFreeChunk(Allocator* a, ChiChunk* chunk) {
    UnmapAllocator* ua = (UnmapAllocator*)a;
    if (!uaFreeDelayed(ua, chunk))
        uaFreeEmergency(ua);
}

static ChiChunk* uaNewChunk(Allocator* a, const size_t size, const size_t align) {
    UnmapAllocator* ua = (UnmapAllocator*)a;
    CHI_ASSERT(chiChunkSizeValid(size));
    CHI_ASSERT(chunkAlignValid(align));
    CHI_ASSERT(size >= align);

    for (;;) {
        ChiChunk* chunk = uaReuse(ua, size, align);
        if (chunk)
            return chunk;
        chunk = vaNewChunk(a, size, align);
        if (chunk)
            return chunk;
        if (!uaFreeEmergency(ua))
            return 0;
    }
}

static void uaSetup(UnmapAllocator* ua, const ChiChunkOption* opts) {
    CHI_ZERO_STRUCT(ua);
    vaSetup(&ua->va, opts);
    chiChunkListInit(&ua->list);
    chiCondInit(&ua->cond);
    ua->timeout = opts->unmapTimeout;
    ua->va.a.freeChunk = uaFreeChunk;
    ua->va.a.newChunk = uaNewChunk;
}
#endif

static void nopFreeChunk(Allocator* CHI_UNUSED(a), ChiChunk* CHI_UNUSED(chunk)) {}

void chiChunkSetup(const ChiChunkOption* opts) {
    CHI_ONCE(
#if CHI_UNMAP_TASK_ENABLED
             if (!chiMillisZero(opts->unmapTimeout) && !opts->noUnmap)
                 uaSetup((UnmapAllocator*)ALLOCATOR, opts);
             else
#endif
                 vaSetup((VirtAllocator*)ALLOCATOR, opts);
             if (opts->noUnmap)
                 ALLOCATOR->freeChunk = nopFreeChunk;
         );
}

ChiChunk* chiChunkNew(size_t size, size_t align) {
    ChiChunk* chunk = ALLOCATOR->newChunk(ALLOCATOR, size, align);
    if (!chunk)
        return 0;
    CHI_ASSERT(CHI_CHOICE(CHI_CHUNK_ARENA_ENABLED, chunk->_used, true));
    CHI_ASSERT(chunk->size == size);
    CHI_ASSERT(chunkAligned(chunk->start, align));
    heaptrack_report_alloc(chunk->start, chunk->size);
    return chunk;
}

void chiChunkFree(ChiChunk* chunk) {
    CHI_ASSERT(CHI_CHOICE(CHI_CHUNK_ARENA_ENABLED, chunk->_used, true));
    heaptrack_report_free(chunk->start);
    ALLOCATOR->freeChunk(ALLOCATOR, chunk);
}

#define CHI_OPT_STRUCT ChiChunkOption
CHI_INTERN CHI_OPT_GROUP(chiChunkOptions, "CHUNK",
                         CHI_OPT_FLAG(smallPages, "c:sp", "Use small pages")
                         CHI_OPT_FLAG(noUnmap, "c:nou", "Disable unmapping")
                         CHI_IF(CHI_UNMAP_TASK_ENABLED,
                                CHI_OPT_UINT64(_CHI_UN(Millis, unmapTimeout), 0, 60000,
                                               "c:ut", "Unmapping timeout in ms")))
#undef CHI_OPT_STRUCT
