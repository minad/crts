#pragma once

#include "private.h"

#define CHI_POISON(x, p)             ({ if (CHI_POISON_ENABLED) { (x) = (typeof(x))(p); } ({}); })
#define CHI_POISONED(x, p)           ((x) == (typeof(x))(p))

#if !defined(NDEBUG) && CHI_POISON_ENABLED
#  define CHI_ASSERT_POISONED(x, p)    CHI_ASSERT(CHI_POISONED(x, p))
#  define CHI_ASSERT_NONPOISONED(x, p) CHI_ASSERT(!CHI_POISONED(x, p))
#else
#  define CHI_ASSERT_POISONED(x, p)    ({})
#  define CHI_ASSERT_NONPOISONED(x, p) ({})
#endif

#if CHI_POISON_ENABLED
void chiPoisonMem(void*, uint8_t, size_t);
#else
CHI_INL void chiPoisonMem(void* CHI_UNUSED(x), uint8_t CHI_UNUSED(p), size_t CHI_UNUSED(n)) {}
#endif

#if CHI_POISON_OBJECT_ENABLED
void chiPoisonObject(void*, size_t);
#else
CHI_INL void chiPoisonObject(void* CHI_UNUSED(x), size_t CHI_UNUSED(n)) {}
#endif

#if CHI_POISON_BLOCK_ENABLED
void chiPoisonBlock(void*, uint8_t, size_t);
#else
CHI_INL void chiPoisonBlock(void* CHI_UNUSED(x), uint8_t CHI_UNUSED(p), size_t CHI_UNUSED(n)) {}
#endif
