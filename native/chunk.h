#pragma once

#include "event/defs.h"
#include "option.h"

#define CHI_FOREACH_CHUNK(chunk, chunklist) CHI_LIST_FOREACH(ChiChunk, link, chunk, chunklist)
#define CHI_FOREACH_CHUNK_NODELETE(chunk, chunklist) CHI_LIST_FOREACH_NODELETE(ChiChunk, link, chunk, chunklist)

typedef struct ChiChunkLink_ ChiChunkLink;
struct ChiChunkLink_ { ChiChunkLink *prev, *next; };
typedef struct ChiChunkAddrLink_ ChiChunkAddrLink;
struct ChiChunkAddrLink_ { ChiChunkAddrLink *prev, *next; };

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
typedef struct {
    void*        start;
    size_t       size;
    ChiChunkLink link;
    CHI_IF(CHI_UNMAP_TASK_ENABLED, ChiMillis _unmapTimeout;)
    CHI_IF(CHI_CHUNK_ARENA_ENABLED, ChiChunkAddrLink addrLink; bool _used;)
} ChiChunk;

#define LIST_PREFIX chiChunkList
#define LIST_ELEM   ChiChunk
#include "generic/list.h"

typedef struct {
    CHI_IF(CHI_CHUNK_ARENA_ENABLED, uintptr_t arenaStart; size_t arenaSize;)
    CHI_IF(CHI_UNMAP_TASK_ENABLED, ChiMillis unmapTimeout;)
    bool smallPages;
    bool noUnmap;
} ChiChunkOption;

CHI_INTERN void chiChunkSetup(const ChiChunkOption*);
CHI_INTERN CHI_WU ChiChunk* chiChunkNew(size_t, size_t);
CHI_INTERN void chiChunkFree(ChiChunk*);

CHI_INL CHI_WU bool chiChunkSizeValid(size_t size) {
    return size >= CHI_CHUNK_MIN_SIZE
        && size <= CHI_CHUNK_MAX_SIZE
        && !(size & (CHI_CHUNK_MIN_SIZE - 1));
}

#define CHI_DEFAULT_CHUNK_OPTION                    \
    {                                               \
        .smallPages = false,                        \
        CHI_IF(CHI_UNMAP_TASK_ENABLED,              \
               .unmapTimeout  = chiMillis(100),)    \
        CHI_IF(CHI_CHUNK_ARENA_ENABLED,             \
               .arenaStart = CHI_CHUNK_START,       \
               .arenaSize = CHI_CHUNK_MAX_SIZE)     \
    }

CHI_EXTERN ChiOptionGroup chiChunkOptions;
