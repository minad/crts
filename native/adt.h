#pragma once

#include "private.h"

CHI_INL CHI_WU Chili chiNewFromArrayFlags(uint32_t flags, ChiType type, const Chili* elems, size_t size) {
    CHI_ASSERT(size > 0);
    Chili c = chiNewFlags(type, size, flags);
    for (size_t i = 0; i < size; ++i)
        CHI_IDX(c, i) = elems[i];
    return c;
}

#define chiNewVariadicFlags(flags, type, ...)                           \
    ({                                                                  \
        Chili _elems[] = { __VA_ARGS__ };                               \
        CHI_ASSERT(CHI_DIM(_elems) > 0);                                \
        chiNewFromArrayFlags((flags), (type), _elems, CHI_DIM(_elems)); \
    })

#define chiNewVariadic(type, ...)     chiNewVariadicFlags(CHI_NEW_DEFAULT, (type), ##__VA_ARGS__)
#define chiNewTagged(tag, ...)        chiNewVariadic(CHI_TAG(tag), __VA_ARGS__)
#define chiNewTuple(...)              chiNewTagged(0, __VA_ARGS__)
#define chiNewFn(arity, cont, ...)    chiNewVariadic(CHI_FN(arity), chiFromCont(cont), ##__VA_ARGS__)
#define chiNewPinVariadic(type, ...)  chiNewVariadicFlags(CHI_NEW_PIN, (type), ##__VA_ARGS__)
#define chiNewPinTagged(tag, ...)     chiNewPinVariadic(CHI_TAG(tag), __VA_ARGS__)
#define chiNewPinTuple(...)           chiNewPinTagged(0, __VA_ARGS__)
#define chiNewPinFn(arity, cont, ...) chiNewPinVariadic(CHI_FN(arity), chiFromCont(cont), ##__VA_ARGS__)

// Maybe
CHI_INL CHI_WU Chili chiNewJust(Chili x)    { return chiNewTuple(x); }

// Either
CHI_INL CHI_WU Chili chiNewLeft(Chili x)    { return chiNewTagged(0, x); }
CHI_INL CHI_WU Chili chiNewRight(Chili x)   { return chiNewTagged(1, x); }
CHI_INL CHI_WU bool  chiLeft(Chili c)       { return chiTag(c) == 0; }
CHI_INL CHI_WU bool  chiRight(Chili c)      { return chiTag(c) == 1; }
CHI_INL CHI_WU bool  chiFromLeft(Chili* c)  { bool l = chiLeft(*c);  if (l) *c = CHI_IDX(*c, 0); return l; }
CHI_INL CHI_WU bool  chiFromRight(Chili* c) { bool r = chiRight(*c); if (r) *c = CHI_IDX(*c, 0); return r; }
CHI_INL CHI_WU bool  chiEither(Chili* c)    { bool r = chiRight(*c); *c = CHI_IDX(*c, 0); return r; }
