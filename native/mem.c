#include <stdlib.h>
#include "mem.h"
#include "error.h"
#include "system.h"

/* Enable primitive malloc implementation.
 * Large requests are passed directly to the chunk allocator.
 * Small requests are handled by power of two sized memory pools.
 * Every allocation has a 16 byte header and allocations are 16 byte aligned.
 * Everything is protected by a single mutex.
 * This implementation will be (usually) used only in the standalone
 * mode, which is single threaded.
 */
#if CHI_MALLOC_ENABLED

#include "chunk.h"
#include "pool.h"
#include "num.h"

#if CHI_VALGRIND_ENABLED
#  include <valgrind/memcheck.h>
#else
#  define VALGRIND_MALLOCLIKE_BLOCK(addr, size, rz, zeroed) ({})
#  define VALGRIND_FREELIKE_BLOCK(addr, rz) ({})
#endif

enum { MALLOC_POOL_SHIFT = 5, MALLOC_HEADER_SIZE = 16 };
static ChiPool mallocPool[CHI_CHUNK_MIN_SHIFT - MALLOC_POOL_SHIFT];
static ChiMutex mallocMutex;

CHI_INL ChiChunk* mallocGetHeader(const void* start, size_t* size) {
    const uint64_t* header = (const uint64_t*)start;
    ChiChunk* chunk = (ChiChunk*)(uintptr_t)header[0];
    *size = (size_t)header[1];
    CHI_ASSERT(!chunk || chunk->start == header);
    return chunk;
}

CHI_INL void* mallocSetHeader(void* start, ChiChunk* chunk, size_t size) {
    uint64_t* header = (uint64_t*)start;
    header[0] = (uintptr_t)chunk;
    header[1] = size;
    CHI_ASSERT(!chunk || chunk->start == header);
    return (uint8_t*)start + MALLOC_HEADER_SIZE;
}

CHI_INL size_t mallocMaxSize(void* mem, size_t* oldSize) {
    ChiChunk* chunk = mallocGetHeader((uint8_t*)mem - MALLOC_HEADER_SIZE, oldSize);
    return chunk ? chunk->size - MALLOC_HEADER_SIZE : *oldSize;
}

CHI_INL uint32_t mallocClass(size_t size) {
    if (size <= (1U << MALLOC_POOL_SHIFT))
        return 0;
    uint32_t cls = chiLog2(size);
    if ((1U << cls) < size)
        ++cls;
    cls -= MALLOC_POOL_SHIFT;
    CHI_ASSERT(cls < CHI_DIM(mallocPool));
    return cls;
}

CHI_INL ChiPool* mallocGetPool(size_t size) {
    ChiPool* pool = mallocPool + mallocClass(size + MALLOC_HEADER_SIZE);
    CHI_ASSERT(pool->object >= size + MALLOC_HEADER_SIZE);
    return pool;
}

static void* mallocLarge(size_t size) {
    size_t chunkSize = CHI_ROUNDUP(size + MALLOC_HEADER_SIZE, CHI_CHUNK_MIN_SIZE);
    ChiChunk* chunk = chiChunkNew(chunkSize, 0);
    return chunk ? mallocSetHeader(chunk->start, chunk, size) : 0;
}

static void* mallocSmall(size_t size) {
    CHI_LOCK(&mallocMutex);
    ChiPool* pool = mallocGetPool(size);
    if (!pool->avail) {
        ChiChunk* chunk = chiChunkNew(CHI_CHUNK_MIN_SIZE, 0);
        if (!chunk)
            return 0;
        chiPoolGrow(pool, chunk->start, CHI_CHUNK_MIN_SIZE);
    }
    return mallocSetHeader(chiPoolGet(pool), 0, size);
}

static void* chi_malloc(size_t size) {
    void* mem = size + MALLOC_HEADER_SIZE <= CHI_CHUNK_MIN_SIZE / 2 ? mallocSmall(size) : mallocLarge(size);
    VALGRIND_MALLOCLIKE_BLOCK(mem, size, 0, 0);
    return mem;
}

static void chi_free(void* mem) {
    if (mem) {
        size_t size;
        uint8_t* start = (uint8_t*)mem - MALLOC_HEADER_SIZE;
        ChiChunk* chunk = mallocGetHeader(start, &size);
        if (chunk) {
            chiChunkFree(chunk);
        } else {
            ChiPool* pool = mallocGetPool(size);
            CHI_LOCK(&mallocMutex);
            chiPoolPut(pool, start);
        }
    }
    VALGRIND_FREELIKE_BLOCK(mem, 0);
}

static void* chi_calloc(size_t count, size_t size) {
    size_t n;
    if (CHI_UNLIKELY(__builtin_mul_overflow(count, size, &n)))
        return 0;
    void* p = chi_malloc(n);
    if (p)
        memset(p, 0, n);
    return p;
}

static void* chi_realloc(void* mem, size_t size) {
    if (!mem)
        return chi_malloc(size);
    if (!size) {
        chi_free(mem);
        return 0;
    }
    size_t oldSize;
    if (mallocMaxSize(mem, &oldSize) >= size) {
        *((uint64_t*)mem - 1) = size;
        return mem;
    }
    void* p = chi_malloc(size);
    if (p) {
        memcpy(p, mem, oldSize);
        chi_free(mem);
    }
    return p;
}

CHI_EARLY_CONSTRUCTOR {
    chiMutexInit(&mallocMutex);
    for (uint32_t cls = 0; cls < CHI_DIM(mallocPool); ++cls) {
        size_t size = 1U << (cls + MALLOC_POOL_SHIFT);
        CHI_ASSERT(mallocClass(size) == cls);
        chiPoolInit(mallocPool + cls, size);
    }
}

#if !CHI_SYSTEM_HAS_MALLOC
void* malloc(size_t size) { return chi_malloc(size); }
void* calloc(size_t count, size_t size) { return chi_calloc(count, size); }
void* realloc(void* mem, size_t size) { return chi_realloc(mem, size); }
void free(void* mem) { return chi_free(mem); }
#endif

#else
_Static_assert(CHI_SYSTEM_HAS_MALLOC, "System does not have malloc. Enable Chili malloc by setting CHI_MALLOC_ENABLED.");
static void* chi_malloc(size_t size) { return malloc(size); }
static void* chi_calloc(size_t count, size_t size) { return calloc(count, size); }
static void* chi_realloc(void* mem, size_t size) { return realloc(mem, size); }
static void chi_free(void* mem) { return free(mem); }
#endif

static void* checkCanary(void* p) {
    if (CHI_POISON_ENABLED && p) {
        uint8_t* q = (uint8_t*)p - 16;
        uint64_t size = chiPeekUInt64(q);
        CHI_ASSERT(chiPeekUInt64(q + 8) == CHI_POISON_MEM_BEGIN);
        CHI_ASSERT(chiPeekUInt64(q + 16 + size) == CHI_POISON_MEM_END);
        return q;
    }
    return p;
}

static size_t addCanary(size_t size) {
    return CHI_POISON_ENABLED ? size + 24 : size;
}

static void* checkAlloc(const char* fn, void* p, size_t size) {
    if (CHI_UNLIKELY(!p && size))
        chiErr("%s failed for size=%Z: %m", fn, size);
    if (CHI_POISON_ENABLED && p) {
        uint8_t* q = (uint8_t*)p;
        chiPokeUInt64(q, size);
        chiPokeUInt64(q + 8, CHI_POISON_MEM_BEGIN);
        chiPokeUInt64(q + 16 + size, CHI_POISON_MEM_END);
        return q + 16;
    }
    return p;
}

static size_t checkOverflow(size_t count, size_t size) {
    size_t res;
    if (CHI_UNLIKELY(__builtin_mul_overflow(count, size, &res)))
        chiErr("Allocation size overflow count=%zu size=%Z", count, size);
    return res;
}

/**
 * Memory allocation which never fails.
 * Only useful for small temporary alloctions within the runtime system.
 */
void* chiAlloc(size_t size) {
    return checkAlloc("malloc", chi_malloc(addCanary(size)), size);
}

/**
 * Memory allocation which never fails
 * and initializes the memory to zero.
 */
void* chiZalloc(size_t size) {
    return checkAlloc("calloc", chi_calloc(addCanary(size), 1), size);
}

/**
 * Reallocation operation which never fails.
 */
void* chiRealloc(void* p, size_t size) {
    return checkAlloc("realloc", chi_realloc(checkCanary(p), size ? addCanary(size) : 0), size);
}

/**
 * Return memory to the allocator.
 */
void chiFree(void* p) {
    chi_free(checkCanary(p));
}

void* chiAllocElems(size_t nelems, size_t size) {
    return chiAlloc(checkOverflow(nelems, size));
}

void* chiZallocElems(size_t nelems, size_t size) {
    return chiZalloc(checkOverflow(nelems, size));
}

void* chiReallocElems(void* p, size_t nelems, size_t size) {
    return chiRealloc(p, checkOverflow(nelems, size));
}

char* chiCStringAlloc(size_t n) {
    char* s = (char*)chiAlloc(n + 1);
    s[n] = 0;
    return s;
}

char* chiCStringUnsafeDup(ChiStringRef t) {
    char* s = chiCStringAlloc(t.size);
    memcpy(s, t.bytes, t.size);
    return s;
}

char* chiCStringDup(const char* s) {
    return chiCStringUnsafeDup(chiStringRef(s));
}
