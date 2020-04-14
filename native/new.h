#pragma once

#include "processor.h"

#define chiNewVariadicFlags(proc, type, flags, ...)                     \
    ({                                                                  \
        Chili _elems[] = { __VA_ARGS__ };                               \
        CHI_ASSERT(CHI_DIM(_elems) > 0);                                \
        chiNewFromArrayFlags((proc), (type), (flags), _elems, CHI_DIM(_elems)); \
    })

#define chiNewVariadic(proc, type, ...)          chiNewVariadicFlags((proc), (type), CHI_NEW_DEFAULT, ##__VA_ARGS__)
#define chiNewTagged(proc, tag, ...)             chiNewVariadic((proc), CHI_TAG(tag), ##__VA_ARGS__)
#define chiNewTuple(proc, ...)                   chiNewTagged((proc), 0, ##__VA_ARGS__)
#define chiNewFn(proc, arity, cont, ...)         chiNewVariadic((proc), CHI_FN(arity), chiFromCont(cont), ##__VA_ARGS__)

#define chiNewTaggedFlags(proc, tag, flags, ...) chiNewVariadicFlags((proc), CHI_TAG(tag), (flags), ##__VA_ARGS__)
#define chiNewTupleFlags(proc, flags, ...)       chiNewTaggedFlags((proc), 0, (flags), ##__VA_ARGS__)
#define chiNewFnFlags(proc, arity, cont, ...)    chiNewVariadicFlags((proc), CHI_FN(arity), (flags), chiFromCont(cont), ##__VA_ARGS__)

#define _chiStringNew(r, proc, s)                ({ ChiStringRef r = chiStringRef(s); chiStringFromBytesFlags((proc), r.bytes, r.size, CHI_NEW_DEFAULT); })
#define chiStringNew(proc, s)                    _chiStringNew(CHI_GENSYM, (proc), (s))

typedef struct ChiProcessor_ ChiProcessor;

CHI_INTERN CHI_WU Chili chiArrayNewUninitialized(ChiProcessor*, uint32_t, ChiNewFlags);
CHI_INTERN CHI_WU Chili chiBufferNewUninitialized(ChiProcessor*, uint32_t, ChiNewFlags);
CHI_INTERN CHI_WU Chili chiStringFromBytesFlags(ChiProcessor*, const uint8_t*, uint32_t, ChiNewFlags);
CHI_INTERN CHI_WU Chili chiNewMajorClean(ChiProcessor*, ChiType, size_t, ChiNewFlags);
CHI_INTERN CHI_WU Chili chiNewMajorDirty(ChiProcessor*, ChiType, size_t, ChiNewFlags);

CHI_INL CHI_WU Chili chiNewInl(ChiProcessor* proc, ChiType type, size_t size, ChiNewFlags flags) {
    CHI_ASSERT(size);
    if (CHI_LIKELY(!(flags & (CHI_NEW_LOCAL | CHI_NEW_SHARED | CHI_NEW_CLEAN)))) {
        if (CHI_LIKELY(size <= CHI_MAX_UNPINNED))
            return chiWrap(chiBlockAllocNew(&proc->heap.nursery.alloc, size), size, type, CHI_GEN_NURSERY);
        flags |= chiRaw(type) ? (CHI_NEW_SHARED | CHI_NEW_CLEAN) : CHI_NEW_LOCAL;
    }
    return flags & CHI_NEW_CLEAN ? chiNewMajorClean(proc, type, size, flags) : chiNewMajorDirty(proc, type, size, flags);
}

CHI_INL CHI_WU Chili chiNewFromArrayFlags(ChiProcessor* proc, ChiType type,
                                          ChiNewFlags flags, const Chili* elems, size_t size) {
    CHI_ASSERT(size > 0);
    Chili c = chiNewInl(proc, type, size, flags);
    for (size_t i = 0; i < size; ++i)
        chiInit(c, chiPayload(c) + i, elems[i]);
    return c;
}

// Maybe
CHI_INL CHI_WU Chili chiNewJust(ChiProcessor* proc, Chili x) {
    return chiNewTuple(proc, x);
}

// Either
CHI_INL CHI_WU Chili chiNewLeft(ChiProcessor* proc, Chili x) {
    return chiNewTagged(proc, 0, x);
}

CHI_INL CHI_WU Chili chiNewRight(ChiProcessor* proc, Chili x) {
    return chiNewTagged(proc, 1, x);
}

CHI_INL CHI_WU bool  chiLeft(Chili c)       { return chiTag(c) == 0; }
CHI_INL CHI_WU bool  chiRight(Chili c)      { return chiTag(c) == 1; }
CHI_INL CHI_WU bool  chiFromLeft(Chili* c)  { bool l = chiLeft(*c);  if (l) *c = chiIdx(*c, 0); return l; }
CHI_INL CHI_WU bool  chiFromRight(Chili* c) { bool r = chiRight(*c); if (r) *c = chiIdx(*c, 0); return r; }
CHI_INL CHI_WU bool  chiEither(Chili* c)    { bool r = chiRight(*c); *c = chiIdx(*c, 0); return r; }
