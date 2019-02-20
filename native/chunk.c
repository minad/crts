#include "chunk.h"
#include "error.h"
#include "num.h"
#include "system.h"
#include "pool.h"
#include "rtoption.h"

#if CHI_HEAPTRACK_ENABLED
#  include <heaptrack_api.h>
#else
#  define heaptrack_report_alloc(x, y) ({})
#  define heaptrack_report_free(x)     ({})
#endif

typedef struct ChunkAllocator_ ChunkAllocator;

static ChiChunk* chunkGetHandle(ChunkAllocator*, void*, size_t);
static ChiChunk* chunkShrinkBegin(ChunkAllocator*, ChiChunk*, size_t);
static ChiChunk* chunkShrinkEnd(ChunkAllocator*, ChiChunk*, size_t);
static void chunkFree(ChunkAllocator*, ChiChunk*);
static bool chunkAlignValid(size_t);
static void chunkNeedHandles(ChunkAllocator*, size_t);
static int32_t chunkFlags(ChunkAllocator*, size_t);

typedef struct {
    ChiCond   cond;
    ChiList   list;
    ChiTask   task;
    ChiMillis timeout;
} Unmap;

struct ChunkAllocator_ {
    Unmap     unmap[CHI_UNMAP_TASK_ENABLED];
    ChiPool   handlePool;
    ChiMutex  mutex;
    bool      smallPages, noUnmap;
#if CHI_CHUNK_ARENA_ENABLED
    ChiList   sizeList[CHI_CHUNK_MAX + 1];
    ChiList   addrList;
    ChiChunk  arena;
#endif
};

static ChunkAllocator chunkAllocator;

_Static_assert(CHI_CHUNK_ARENA_ENABLED || !CHI_CHUNK_START, "If chunk arena is disabled, chunks start at 0.");
_Static_assert(CHI_CHUNK_ARENA_ENABLED || !CHI_CHUNK_ARENA_FIXED, "Fixed arena requires enabled arena.");
_Static_assert(CHI_CHUNK_ARENA_FIXED || !CHI_CHUNK_START, "Fixed arena must not start at 0.");
_Static_assert(CHI_SYSTEM_HAS_TASKS || !CHI_UNMAP_TASK_ENABLED, "Unmap task can only be enabled if the system has support for tasks");

#if CHI_CHUNK_ARENA_ENABLED

CHI_INL CHI_WU uint32_t chunkClassSmaller(size_t size) {
    uint32_t cls = chiLog2(size);
    CHI_ASSERT(cls >= CHI_CHUNK_MIN_SHIFT);
    CHI_ASSERT(cls <= CHI_CHUNK_MAX_SHIFT);
    return (uint32_t)(cls - CHI_CHUNK_MIN_SHIFT);
}

CHI_INL CHI_WU uint32_t chunkClassLarger(size_t size) {
    uint32_t cls = chunkClassSmaller(size);
    return (1ULL << (cls + CHI_CHUNK_MIN_SHIFT)) < size ? cls + 1 : cls;
}

static void chunkCoalesce(ChunkAllocator* alloc, ChiChunk* chunk, ChiChunk* s) {
    CHI_ASSERT((uint8_t*)chunk->start + chunk->size == s->start);
    chiListDelete(&s->_addrList);
    chunk->size += s->size;
    chiPoolPut(&alloc->handlePool, s);
}

static void chunkInsertFree(ChunkAllocator* alloc, ChiChunk* chunk) {
    CHI_ASSERT(chunk->_used);
    chunk->_used = false;

    ChiChunk *before = chunk->_addrList.prev != &alloc->addrList ? CHI_OUTER(ChiChunk, _addrList, chunk->_addrList.prev) : 0;
    if (before && !before->_used) {
        chiListDelete(&before->list);
        chunkCoalesce(alloc, before, chunk);
        chunk = before;
    }

    ChiChunk *after = chunk->_addrList.next != &alloc->addrList ? CHI_OUTER(ChiChunk, _addrList, chunk->_addrList.next) : 0;
    if (after && !after->_used) {
        chiListDelete(&after->list);
        chunkCoalesce(alloc, chunk, after);
    }

    chiListAppend(alloc->sizeList + chunkClassSmaller(chunk->size), &chunk->list);
}

static void chunkFreeImmediately(ChunkAllocator* alloc, ChiChunk* chunk) {
    CHI_ASSERT(chunk->_used);
    chiVirtFree(chunk->start, chunk->size);
    CHI_LOCK(&alloc->mutex);
    chunkInsertFree(alloc, chunk);
    heaptrack_report_free(chunk->start);
}

static void chunkSetupArena(ChunkAllocator* alloc, uintptr_t arenaStart, size_t arenaSize) {
    CHI_ASSERT(chunkClassSmaller(CHI_CHUNK_MAX_SIZE) == CHI_CHUNK_MAX);
    CHI_ASSERT(arenaSize >= CHI_CHUNK_MIN_SIZE);

    chiListInit(&alloc->addrList);
    for (uint32_t i = 0; i <= CHI_CHUNK_MAX; ++i)
        chiListInit(alloc->sizeList + i);

    chiPoolPut(&alloc->handlePool, &alloc->arena);
    ChiChunk* chunk = chunkGetHandle(alloc, (void*)arenaStart, arenaSize);
    chiListAppend(alloc->sizeList + chunkClassSmaller(chunk->size), &chunk->list);
    chiListAppend(&alloc->addrList, &chunk->_addrList);
}

static ChiChunk* chunkFindSize(ChunkAllocator* alloc, const size_t size) {
    uint32_t cls = chunkClassLarger(size);
    while (cls <= CHI_CHUNK_MAX && chiListNull(alloc->sizeList + cls))
        ++cls;
    if (cls == CHI_CHUNK_MAX + 1) {
        CHI_IF(CHI_SYSTEM_HAS_ERRNO, errno = ENOMEM);
        return 0;
    }
    ChiChunk* chunk = chiChunkPop(alloc->sizeList + cls);
    CHI_ASSERT(chunk->size >= size);
    CHI_ASSERT(chunkClassSmaller(chunk->size) == cls);
    CHI_ASSERT(!chunk->_used);
    chunk->_used = true;
    return chunk;
}

static ChiChunk* chunkFind(ChunkAllocator* alloc, size_t size, size_t align) {
    CHI_LOCK(&alloc->mutex);
    chunkNeedHandles(alloc, 2);
    ChiChunk *chunk = chunkFindSize(alloc, size + align);
    if (!chunk)
        return 0;
    ChiChunk *before = chunkShrinkBegin(alloc, chunk, align), *after = chunkShrinkEnd(alloc, chunk, size);
    if (before)
        chunkInsertFree(alloc, before);
    if (after)
        chunkInsertFree(alloc, after);
    return chunk;
}

static ChiChunk* chunkFresh(ChunkAllocator* alloc, const size_t size, const size_t align) {
    ChiList failed = CHI_LIST_INIT(failed);
    ChiChunk* chunk = 0;
    for (uint32_t tries = 0; tries < CHI_CHUNK_MEMMAP_TRIES; ++tries) {
        chunk = chunkFind(alloc, size, align);
        if (!chunk)
            break;

        if (chiVirtAlloc(chunk->start, chunk->size, chunkFlags(alloc, chunk->size)))
            break;

        chiListAppend(&failed, &chunk->list);
        chunk = 0;
    }

    CHI_LOCK(&alloc->mutex);
    while (!chiListNull(&failed))
        chunkInsertFree(alloc, chiChunkPop(&failed));

    return chunk;
}

static bool chunkReserveHandles(ChunkAllocator* alloc) {
    ChiList failed = CHI_LIST_INIT(failed);
    ChiChunk* chunk = 0;
    for (uint32_t tries = 0; tries < CHI_CHUNK_MEMMAP_TRIES; ++tries) {
        chunk = chunkFindSize(alloc, CHI_CHUNK_MIN_SIZE);
        if (!chunk)
            break;
        if (chiVirtAlloc(chunk->start, CHI_CHUNK_MIN_SIZE, 0))
            break;
        chiListAppend(&failed, &chunk->list);
        chunk = 0;
    }

    while (!chiListNull(&failed))
        chunkInsertFree(alloc, chiChunkPop(&failed));

    if (!chunk)
        return false;

    chiPoolGrow(&alloc->handlePool, chunk->start, CHI_CHUNK_MIN_SIZE);
    ChiChunk *after = chunkShrinkEnd(alloc, chunk, CHI_CHUNK_MIN_SIZE);
    if (after)
        chunkInsertFree(alloc, after);

    return true;
}

#else

static void chunkFreeImmediately(ChunkAllocator* alloc, ChiChunk* chunk) {
    chiVirtFree(chunk->start, chunk->size);
    CHI_LOCK(&alloc->mutex);
    chiPoolPut(&alloc->handlePool, chunk);
    heaptrack_report_free(chunk->start);
}

static ChiChunk* chunkFresh(ChunkAllocator* alloc, const size_t size, const size_t align) {
    for (uint32_t tries = 0; tries < CHI_CHUNK_MEMMAP_TRIES; ++tries) {
        void* start = chiVirtAlloc(0, size + align, chunkFlags(alloc, size));
        if (!start)
            break;
        void* newStart = align ? (void*)CHI_ROUNDUP((uintptr_t)start, align) : start;
        if (newStart != start) {
            chiVirtFree(start, size + align);
            start = chiVirtAlloc(newStart, size, chunkFlags(alloc, size));
            if (!start)
                continue;
        }
        CHI_LOCK(&alloc->mutex);
        chunkNeedHandles(alloc, 1);
        return chunkGetHandle(alloc, start, size);
    }
    return 0;
}

static bool chunkReserveHandles(ChunkAllocator* alloc) {
    void* start = chiVirtAlloc(0, CHI_CHUNK_MIN_SIZE, 0);
    if (!start)
        return false;
    chiPoolGrow(&alloc->handlePool, start, CHI_CHUNK_MIN_SIZE);
    return true;
}

static void chunkSetupArena(ChunkAllocator* CHI_UNUSED(alloc), uintptr_t CHI_UNUSED(start), size_t CHI_UNUSED(size)) {
}
#endif

static int32_t chunkFlags(ChunkAllocator* alloc, size_t size) {
    return CHI_MEMMAP_NORESERVE |
        (size < CHI_MiB(2) ? CHI_MEMMAP_NOHUGE : 0) |
        (size >= CHI_MiB(2) && alloc->smallPages ? 0 : CHI_MEMMAP_HUGE);
}

static void chunkNeedHandles(ChunkAllocator* alloc, size_t needed) {
    if (needed > alloc->handlePool.avail && !chunkReserveHandles(alloc))
        chiErr("Failed to reserve memory for chunk handles: %m");
}

static ChiChunk* chunkGetHandle(ChunkAllocator* alloc, void* start, size_t size) {
    ChiChunk* chunk = (ChiChunk*)chiPoolGet(&alloc->handlePool);
#if CHI_CHUNK_ARENA_ENABLED
    chunk->_used = false;
    chiListPoison(&chunk->_addrList);
#endif
    chiListPoison(&chunk->list);
    chunk->size = size;
    chunk->start = start;
    chunk->_timeout = (ChiMillis){0};
    return chunk;
}

static bool chunkAlignValid(size_t align) {
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

static ChiChunk* chunkShrinkBegin(ChunkAllocator* alloc, ChiChunk* chunk, size_t align) {
    if (!chunkAligned(chunk->start, align)) {
        uintptr_t oldStart = (uintptr_t)chunk->start, newStart = CHI_ROUNDUP(oldStart, align);
        ChiChunk* before = chunkGetHandle(alloc, chunk->start, newStart - oldStart);
        chunk->start = (void*)newStart;
        CHI_ASSERT(chunk->size > before->size);
        chunk->size -= before->size;
#if CHI_CHUNK_ARENA_ENABLED
        before->_used = true;
        chiListAppend(&chunk->_addrList, &before->_addrList);
#endif
        return before;
    }
    return 0;
}

static ChiChunk* chunkShrinkEnd(ChunkAllocator* alloc, ChiChunk* chunk, size_t size) {
    if (chunk->size != size) {
        CHI_ASSERT(chunk->size > size);
        ChiChunk* after = chunkGetHandle(alloc, (void*)((uintptr_t)chunk->start + size), chunk->size - size);
        CHI_ASSERT(chunk->size > after->size);
        chunk->size -= after->size;
#if CHI_CHUNK_ARENA_ENABLED
        after->_used = true;
        chiListPrepend(&chunk->_addrList, &after->_addrList);
#endif
        return after;
    }
    return 0;
}

static void chunkShrink(ChunkAllocator* alloc, ChiChunk* chunk, size_t size, size_t align) {
    if (chunk->size != size) {
        ChiChunk* before, *after;
        {
            CHI_LOCK(&alloc->mutex);
            chunkNeedHandles(alloc, 2);
            before = chunkShrinkBegin(alloc, chunk, align);
            after = chunkShrinkEnd(alloc, chunk, size);
        }
        if (before)
            chunkFree(alloc, before);
        if (after)
            chunkFree(alloc, after);
    }
}

// Try to reuse recently freed chunk to reduce pressure
// on the kernel memory allocator
static ChiChunk* unmapReuseMatching(ChunkAllocator* alloc, size_t size, size_t align) {
    CHI_LOCK(&alloc->mutex);
    CHI_FOREACH_CHUNK_NODELETE(chunk, &alloc->unmap->list) {
        if (chunk->size == size && chunkAligned(chunk->start, align)) {
            chiListDelete(&chunk->list);
            return chunk;
        }
    }
    return 0;
}

static ChiChunk* unmapReuseAny(ChunkAllocator* alloc, size_t size) {
    CHI_LOCK(&alloc->mutex);
    CHI_FOREACH_CHUNK_NODELETE(chunk, &alloc->unmap->list) {
        if (chunk->size >= size) {
            chiListDelete(&chunk->list);
            return chunk;
        }
    }
    return 0;
}

static ChiMillis currentMillis(void) {
    return chiNanosToMillis(chiClock(CHI_CLOCK_REAL_FAST));
}

static ChiChunk* unmapPopWait(ChunkAllocator* alloc) {
    CHI_LOCK(&alloc->mutex);
    for (;;) {
        if (!chiListNull(&alloc->unmap->list)) {
            ChiChunk* chunk = CHI_OUTER(ChiChunk, list, alloc->unmap->list.next);
            if (chiMillisLess(chunk->_timeout, currentMillis())) {
                chiListDelete(&chunk->list);
                return chunk;
            }
        }
        chiCondWait(&alloc->unmap->cond, &alloc->mutex);
    }
}

static ChiChunk* unmapPopNowait(ChunkAllocator* alloc) {
    CHI_LOCK(&alloc->mutex);
    return chiListNull(&alloc->unmap->list) ? 0 : chiChunkPop(&alloc->unmap->list);
}

static void unmapRun(void* arg) {
    ChunkAllocator* alloc = (ChunkAllocator*)arg;
    chiTaskName("unmap");
    ChiChunk* chunk;
    while ((chunk = unmapPopWait(alloc)))
        chunkFreeImmediately(alloc, chunk);
}

static void unmapSetup(ChunkAllocator* alloc, ChiMillis timeout) {
    chiListInit(&alloc->unmap->list);
    chiCondInit(&alloc->unmap->cond);
    alloc->unmap->timeout = timeout;
    if (!chiMillisZero(alloc->unmap->timeout)     // no immediate unmapping
        && !alloc->noUnmap)     // unmapping requested
        alloc->unmap->task = chiTaskCreate(unmapRun, alloc);
}

static void chunkSetup(ChunkAllocator* alloc, const ChiChunkOption* opts) {
    CHI_CLEAR(alloc);
    chiMutexInit(&alloc->mutex);
    CHI_LOCK(&alloc->mutex);
    alloc->smallPages = opts->smallPages;
    alloc->noUnmap = opts->noUnmap;
    chiPoolInit(&alloc->handlePool, sizeof (ChiChunk));
    chunkSetupArena(alloc, opts->arenaStart, opts->arenaSize);
    if (CHI_UNMAP_TASK_ENABLED)
        unmapSetup(alloc, opts->unmapTimeout);
}

static void unmapPush(ChunkAllocator* alloc, ChiChunk* chunk) {
    ChiMillis time = chiMillisAdd(currentMillis(), alloc->unmap->timeout);
    chunk->_timeout = chiMillisAdd(time, alloc->unmap->timeout);
    CHI_LOCK(&alloc->mutex);
    chiListAppend(&alloc->unmap->list, &chunk->list);
    if (chiMillisLess(CHI_OUTER(ChiChunk, list, alloc->unmap->list.next)->_timeout, time))
        chiCondSignal(&alloc->unmap->cond);
}

static void chunkFree(ChunkAllocator* alloc, ChiChunk* chunk) {
#if CHI_CHUNK_ARENA_ENABLED
    CHI_ASSERT(chunk->_used);
#endif
    if (!alloc->noUnmap) {
        if (CHI_UNMAP_TASK_ENABLED && !chiTaskNull(alloc->unmap->task))
            unmapPush(alloc, chunk);
        else
            chunkFreeImmediately(alloc, chunk);
    }
}

static bool unmapImmediately(ChunkAllocator* alloc) {
    ChiChunk* chunk;
    while ((chunk = unmapPopNowait(alloc)))
        chunkFreeImmediately(alloc, chunk);
    return !!chunk;
}

static ChiChunk* chunkNew(ChunkAllocator* alloc, const size_t size, const size_t align) {
    CHI_ASSERT(chiChunkSizeValid(size));
    CHI_ASSERT(chunkAlignValid(align));
    for (;;) {
        ChiChunk* chunk = CHI_UNMAP_TASK_ENABLED ? unmapReuseMatching(alloc, size, align) : 0;
        if (chunk)
            return chunk;
        chunk = chunkFresh(alloc, size, align);
        if (chunk)
            return chunk;
        if (!CHI_UNMAP_TASK_ENABLED)
            return 0;
        if (CHI_SYSTEM_HAS_PARTIAL_VIRTFREE) {
            chunk = unmapReuseAny(alloc, size + align);
            if (chunk) {
                chunkShrink(alloc, chunk, size, align);
                return chunk;
            }
        }
        if (!unmapImmediately(alloc))
            return 0;
    }
}

void chiChunkSetup(const ChiChunkOption* opts) {
    chunkSetup(&chunkAllocator, opts);
}

ChiChunk* chiChunkNew(size_t size, size_t align) {
    ChiChunk* chunk = chunkNew(&chunkAllocator, size, align);
    if (!chunk)
        return 0;
#if CHI_CHUNK_ARENA_ENABLED
    CHI_ASSERT(chunk->_used);
#endif
    CHI_ASSERT(chunk->size == size);
    CHI_ASSERT(chunkAligned(chunk->start, align));
    heaptrack_report_alloc(chunk->start, chunk->size);
    return chunk;
}

void chiChunkFree(ChiChunk* chunk) {
    chunkFree(&chunkAllocator, chunk);
}

#define CHI_OPT_TARGET ChiChunkOption
const ChiOption chiChunkOptionList[] = {
    CHI_OPT_DESC_TITLE("CHUNK ALLOCATOR")
    CHI_OPT_DESC_FLAG(smallPages, "c:sp", "Use small pages")
    CHI_OPT_DESC_FLAG(noUnmap, "c:nou", "Disable unmapping")
    CHI_IF(CHI_UNMAP_TASK_ENABLED, CHI_OPT_DESC_UINT64(_CHI_UN(Millis, unmapTimeout), 0, 60000, "c:ut", "Unmapping timeout in ms"))
    CHI_OPT_DESC_END
};
#undef CHI_OPT_TARGET
