#pragma once

#include "private.h"

#define CHI_POISONED(x, p) ((x) == (typeof(x))(p))

#if CHI_POISON_ENABLED
#  define CHI_ASSERT_POISONED(x, p)    CHI_ASSERT(CHI_POISONED(x, p))
#  define CHI_ASSERT_NONPOISONED(x, p) CHI_ASSERT(!CHI_POISONED(x, p))
#  define CHI_POISON(x, p)             ((x) = (typeof(x))(p))
void chiPoisonMem(void*, uint8_t, size_t);
#else
#  define CHI_ASSERT_POISONED(x, p)    ({})
#  define CHI_ASSERT_NONPOISONED(x, p) ({})
#  define CHI_POISON(x, p)             ({})
CHI_INL void chiPoisonMem(void* CHI_UNUSED(x), uint8_t CHI_UNUSED(p), size_t CHI_UNUSED(n)) {}
#endif

#define CHI_POISON_STRUCT(x, p) chiPoisonMem((x), (p), CHI_SIZEOF_STRUCT(x))
#define CHI_POISON_ARRAY(x, p)  chiPoisonMem((x), (p), CHI_SIZEOF_ARRAY(x))

#if CHI_POISON_OBJECT_ENABLED
void chiPoisonObject(void*, size_t);
#else
CHI_INL void chiPoisonObject(void* CHI_UNUSED(x), size_t CHI_UNUSED(n)) {}
#endif
