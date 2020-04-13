/* Large requests are passed directly to the chunk allocator.
 * Small requests are handled by power of two sized memory pools.
 * Every allocation has a 16 byte header and allocations are 16 byte aligned.
 * Everything is protected by a single mutex.
 * For single threaded backends this does not matter.
 * Usually for multi threaded backends the system allocator is used.
 * Furthermore the Chili runtime does not require good malloc performance
 * since most allocations go through the special block allocator and major heap.
 */
#include "../chunk.h"
#include "../num.h"
#include "../pool.h"

#if CHI_VALGRIND_ENABLED
#  include <valgrind/memcheck.h>
#else
#  define VALGRIND_MALLOCLIKE_BLOCK(addr, size, rz, zeroed) ({})
#  define VALGRIND_FREELIKE_BLOCK(addr, rz) ({})
#endif

enum { MALLOC_POOL_SHIFT = 5, MALLOC_HEADER_SIZE = 16 };
static ChiPool mallocPool[CHI_CHUNK_MIN_SHIFT - MALLOC_POOL_SHIFT];
static ChiMutex mallocMutex;

CHI_INL ChiChunk* mallocGetHeader(const void* mem, size_t* size) {
    const uint64_t* hdr = (const uint64_t*)mem - 2;
    *size = (size_t)hdr[1];
    ChiChunk* chunk = (ChiChunk*)(uintptr_t)hdr[0];
    CHI_ASSERT(!chunk || chunk->start == hdr);
    return chunk;
}

CHI_INL uint32_t mallocClass(size_t size) {
    if (size <= (1UL << MALLOC_POOL_SHIFT))
        return 0;
    uint32_t cls = chiLog2RoundUp(size);
    cls -= MALLOC_POOL_SHIFT;
    CHI_ASSERT(cls < CHI_DIM(mallocPool));
    return cls;
}

CHI_INL ChiPool* mallocGetPool(size_t size) {
    ChiPool* pool = mallocPool + mallocClass(size + MALLOC_HEADER_SIZE);
    CHI_ASSERT(pool->object >= size + MALLOC_HEADER_SIZE);
    return pool;
}

static void* mallocOutOfMem(void) {
    CHI_IF(CHI_SYSTEM_HAS_ERRNO, errno = ENOMEM);
    return 0;
}

static void* mallocLarge(size_t size) {
    size_t chunkSize = CHI_ALIGNUP(size + MALLOC_HEADER_SIZE, CHI_CHUNK_MIN_SIZE);
    ChiChunk* chunk = chiChunkNew(chunkSize, 0);
    if (!chunk)
        return mallocOutOfMem();
    uint64_t* hdr = (uint64_t*)chunk->start;
    hdr[0] = (uintptr_t)chunk;
    hdr[1] = size;
    return hdr + 2;
}

static void* mallocSmall(size_t size) {
    CHI_LOCK_MUTEX(&mallocMutex);
    ChiPool* pool = mallocGetPool(size);
    uint64_t* hdr = (uint64_t*)chiPoolTryGet(pool);
    if (CHI_UNLIKELY(!hdr)) {
        ChiChunk* chunk = chiChunkNew(CHI_CHUNK_MIN_SIZE, 0);
        if (!chunk)
            return mallocOutOfMem();
        chiPoolGrow(pool, chunk->start, CHI_CHUNK_MIN_SIZE);
        hdr = (uint64_t*)chiPoolGet(pool);
    }
    CHI_ASSERT((uintptr_t)hdr % 16 == 0);
    hdr[0] = 0;
    hdr[1] = size;
    return hdr + 2;
}

static void* chi_system_malloc(size_t size) {
    void* mem = CHI_LIKELY(size + MALLOC_HEADER_SIZE <= CHI_CHUNK_MIN_SIZE / 2) ? mallocSmall(size) : mallocLarge(size);
    VALGRIND_MALLOCLIKE_BLOCK(mem, size, 0, 0);
    return mem;
}

static void chi_system_free(void* mem) {
    if (mem) {
        size_t size;
        ChiChunk* chunk = mallocGetHeader(mem, &size);
        if (chunk) {
            chiChunkFree(chunk);
        } else {
            ChiPool* pool = mallocGetPool(size);
            CHI_LOCK_MUTEX(&mallocMutex);
            chiPoolPut(pool, (uint64_t*)mem - 2);
        }
    }
    VALGRIND_FREELIKE_BLOCK(mem, 0);
}

static void* chi_system_calloc(size_t count, size_t size) {
    size_t n;
    if (CHI_UNLIKELY(__builtin_mul_overflow(count, size, &n)))
        return mallocOutOfMem();
    void* p = chi_system_malloc(n);
    if (p)
        memset(p, 0, n);
    return p;
}

static void* chi_system_realloc(void* mem, size_t size) {
    if (!mem)
        return chi_system_malloc(size);
    if (!size) {
        chi_system_free(mem);
        return 0;
    }
    size_t oldSize;
    ChiChunk* chunk = mallocGetHeader(mem, &oldSize);
    size_t maxSize = chunk
        ? chunk->size - MALLOC_HEADER_SIZE
        : (1UL << (mallocClass(oldSize + MALLOC_HEADER_SIZE) + MALLOC_POOL_SHIFT)) - MALLOC_HEADER_SIZE;
    if (maxSize >= size) {
        uint64_t* hdr = (uint64_t*)mem - 2;
        hdr[1] = size;
        return mem;
    }
    void* p = chi_system_malloc(size);
    if (p) {
        memcpy(p, mem, oldSize);
        chi_system_free(mem);
    }
    return p;
}

CHI_EARLY_CONSTRUCTOR(malloc) {
    chiMutexInit(&mallocMutex);
    for (uint32_t cls = 0; cls < CHI_DIM(mallocPool); ++cls) {
        size_t size = 1UL << (cls + MALLOC_POOL_SHIFT);
        CHI_ASSERT(mallocClass(size) == cls);
        chiPoolInit(mallocPool + cls, size);
    }
}
