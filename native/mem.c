#include <stdlib.h>
#include "error.h"
#include "mem.h"
#include "system.h"

#if CHI_SYSTEM_HAS_MALLOC
// lint: no-sort
#  include "malloc/system.h"
#  include "malloc/canary.h"
#else
// lint: no-sort
#  include "malloc/pool.h"
#  include "malloc/canary.h"
#  include "malloc/export.h"
#endif

static void* checkAlloc(const char* fn, void* p, size_t size) {
    if (CHI_UNLIKELY(!p && size))
        chiErr("%s failed for size=%Z: %m", fn, size);
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
    return checkAlloc("malloc", chi_malloc(size), size);
}

/**
 * Memory allocation which never fails
 * and initializes the memory to zero.
 */
void* chiZalloc(size_t size) {
    return checkAlloc("calloc", chi_calloc(1, size), size);
}

/**
 * Reallocation operation which never fails.
 */
void* chiRealloc(void* p, size_t size) {
    return checkAlloc("realloc", chi_realloc(p, size), size);
}

/**
 * Return memory to the allocator.
 */
void chiFree(void* p) {
    chi_free(p);
}

void* chiAllocMany(size_t nelems, size_t size) {
    return chiAlloc(checkOverflow(nelems, size));
}

void* chiZallocMany(size_t nelems, size_t size) {
    return chiZalloc(checkOverflow(nelems, size));
}

void* chiReallocMany(void* p, size_t nelems, size_t size) {
    return chiRealloc(p, checkOverflow(nelems, size));
}

char* chiCStringAlloc(size_t n) {
    char* s = (char*)chiAlloc(n + 1);
    s[n] = 0;
    return s;
}

char* chiCStringUnsafeDup(ChiStringRef t) {
    CHI_ASSERT(!memchr(t.bytes, 0, t.size));
    char* s = chiCStringAlloc(t.size);
    memcpy(s, t.bytes, t.size);
    return s;
}

char* chiCStringDup(const char* s) {
    return chiCStringUnsafeDup(chiStringRef(s));
}
