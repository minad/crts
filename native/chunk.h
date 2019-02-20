#pragma once

#include "list.h"
#include "event/defs.h"

#define CHI_FOREACH_CHUNK(chunk, chunklist) CHI_LIST_FOREACH(ChiChunk, list, chunk, chunklist)
#define CHI_FOREACH_CHUNK_NODELETE(chunk, chunklist) CHI_LIST_FOREACH_NODELETE(ChiChunk, list, chunk, chunklist)

/**
 * The chunk allocator provides chunks with sizes between CHI_CHUNK_MIN_SIZE and CHI_CHUNK_MAX_SIZE.
 * CHI_CHUNK_MIN_SIZE is the allocation granularity of the operating system (4K page size or 64K on Windows).
 * Chunk sizes must be multiples of CHI_CHUNK_MIN_SIZE.
 * Chunks are automatically coalesced and split by the allocator.
 * Chunks live starting at the virtual memory address CHI_CHUNK_START.
 * The desired alignment of chunks can be specified. If no alignment
 * is specified, chunks are at least aligned at the page boundaries,
 * but no assumption should be made.
 *
 * Allocated chunks are taken directly from the operating system
 * and returned lazily in a background thread, since unmapping
 * can be slow. Huge pages are used if the operating system supports them.
 */
typedef struct ChiChunk_ {
    void*     start;
    ChiList   list;
    size_t    size;
    ChiMillis _timeout;
#if CHI_CHUNK_ARENA_ENABLED
    bool      _used;
    ChiList   _addrList;
#endif
} ChiChunk;

typedef struct {
    uintptr_t arenaStart;
    size_t arenaSize;
    bool smallPages;
    bool noUnmap;
    ChiMillis unmapTimeout;
} ChiChunkOption;

void chiChunkSetup(const ChiChunkOption*);
CHI_WU ChiChunk* chiChunkNew(size_t, size_t);
void chiChunkFree(ChiChunk*);

CHI_INL CHI_WU bool chiChunkSizeValid(size_t size) {
    return size >= CHI_CHUNK_MIN_SIZE
        && size <= CHI_CHUNK_MAX_SIZE
        && !(size & (CHI_CHUNK_MIN_SIZE - 1));
}

CHI_INL ChiChunk* chiChunkPop(ChiList* list) {
    return CHI_OUTER(ChiChunk, list, chiListPop(list));
}

#define CHI_DEFAULT_CHUNK_OPTION \
    { \
        .unmapTimeout  = (ChiMillis){1000}, \
        .smallPages    = false, \
        .arenaStart = CHI_CHUNK_START, \
        .arenaSize = CHI_CHUNK_MAX_SIZE \
    }
