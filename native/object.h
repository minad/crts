#pragma once

#include "debug.h"

_Static_assert(sizeof (struct ChiObject_) == _CHI_OBJECT_HEADER_SIZE * CHI_WORDSIZE, "Invalid object header size");

CHI_INTERN CHI_WU const char* chiTypeName(ChiType);

CHI_INL void chiColorStateInit(ChiColorState* s) {
    s->white = CHI_WRAP(Color, _CHI_COLOR_USED);
    s->gray  = CHI_WRAP(Color, _CHI_COLOR_SHADED | _CHI_COLOR_USED);
    s->black = CHI_WRAP(Color, _CHI_COLOR_EPOCH + _CHI_COLOR_USED);
}

CHI_INL void chiColorStateEpoch(ChiColorState* s) {
    s->black = CHI_WRAP(Color, (uint8_t)(CHI_UN(Color, s->black) + _CHI_COLOR_EPOCH));
    s->gray  = CHI_WRAP(Color, (uint8_t)(CHI_UN(Color, s->gray)  + _CHI_COLOR_EPOCH));
    s->white = CHI_WRAP(Color, (uint8_t)(CHI_UN(Color, s->white) + _CHI_COLOR_EPOCH));
}

CHI_INL CHI_WU bool chiColorEq(ChiColor a, ChiColor b) {
    return CHI_UN(Color, a) == CHI_UN(Color, b);
}

CHI_INL bool chiColorUsed(ChiColor c) {
    return !!(CHI_UN(Color, c) & _CHI_COLOR_USED);
}

CHI_INL void chiObjectSetDirty(ChiObject* o, bool set) {
    uint32_t flags = atomic_load_explicit(&o->flags, memory_order_relaxed);
    atomic_store_explicit(&o->flags, (uint8_t)(set ? flags | _CHI_OBJECT_DIRTY : flags & ~(uint32_t)_CHI_OBJECT_DIRTY),
                          memory_order_relaxed);
}

CHI_INL void chiObjectSetSize(ChiObject* o, size_t size) {
    CHI_ASSERT(size <= UINT32_MAX);
    atomic_store_explicit(&o->size, (uint32_t)size, memory_order_relaxed);
}

CHI_INL void chiObjectSetColor(ChiObject* o, ChiColor set) {
    atomic_store_explicit(&o->color, CHI_UN(Color, set), memory_order_relaxed);
}

#if CHI_POISON_ENABLED
CHI_INL void chiObjectSetOwner(ChiObject* o, uint8_t set) {
    atomic_store_explicit(&o->owner, set, memory_order_relaxed);
}
#endif

CHI_INL CHI_WU bool chiObjectTryLock(ChiObject* o) {
    return !atomic_exchange_explicit(&o->lock, true, memory_order_acq_rel);
}

CHI_INL CHI_WU bool chiObjectLocked(ChiObject* o) {
    return atomic_load_explicit(&o->lock, memory_order_relaxed);
}

CHI_INL void chiObjectLock(ChiObject* o) {
    while (!chiObjectTryLock(o));
}

CHI_INL void chiObjectUnlock(ChiObject* o) {
    CHI_ASSERT(chiObjectLocked(o));
    atomic_store_explicit(&o->lock, false, memory_order_release);
}

CHI_DEFINE_AUTO_LOCK(ChiObject, chiObjectLock, chiObjectUnlock)
#define CHI_LOCK_OBJECT(m) CHI_AUTO_LOCK(ChiObject, m)
