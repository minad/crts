#pragma once

#include <chili/object/string.h>

CHI_WU char* chiCStringAlloc(size_t);
CHI_WU char* chiCStringDup(const char*);
// Unsafe since StringRef must not contain '\0'
CHI_WU char* chiCStringUnsafeDup(ChiStringRef);

CHI_MALLOC CHI_WU CHI_RET_NONNULL CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* chiAlloc(size_t);
CHI_MALLOC CHI_WU CHI_RET_NONNULL CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* chiZalloc(size_t);
CHI_WU CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* chiRealloc(void*, size_t);
CHI_MALLOC CHI_WU CHI_RET_NONNULL CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* chiAllocElems(size_t, size_t);
CHI_MALLOC CHI_WU CHI_RET_NONNULL CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* chiZallocElems(size_t, size_t);
CHI_WU CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* chiReallocElems(void*, size_t, size_t);
void chiFree(void*);

#define chiAllocArr(t, n)   ((t*)chiAllocElems((n), sizeof (t)))
#define chiZallocArr(t, n)  ((t*)chiZallocElems((n), sizeof (t)))
#define chiReallocArr(p, n) ((typeof(p))chiReallocElems((p), (n), sizeof (*p)))
#define chiAllocObj(t)      ((t*)chiAlloc(sizeof (t)))
#define chiZallocObj(t)     ((t*)chiZalloc(sizeof (t)))

#define _CHI_AUTO_FREE(tmp, name, init)    CHI_AUTO(tmp, (void*)(init), chiFree); typeof(init) name = (typeof(init))tmp
#define CHI_AUTO_FREE(name, init)          _CHI_AUTO_FREE(CHI_GENSYM, name, init)
#define CHI_AUTO_ALLOC(type, name, nelem)  CHI_AUTO_FREE(name, chiAllocArr(type, (nelem)))
#define CHI_AUTO_ZALLOC(type, name, nelem) CHI_AUTO_FREE(name, chiZallocArr(type, (nelem)))

CHI_DEFINE_AUTO(void*, chiFree)
