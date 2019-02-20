#pragma once

#include "block.h"

enum {
    _CHI_HEADER_OVERHEAD = 1,
};

typedef struct ChiObject_ ChiObject;

/**
 * Classical mark and sweep colors
 *
 * * white - unreachable object or not yet reached
 * * gray  - marked/reachable object, to be scanned
 * * black - marked/reachable object, already scanned
 *
 * Values are chosen such that `max(a, b) = a | b`.
 */
typedef enum {
    CHI_WHITE = 0,
    CHI_GRAY  = 1,
    CHI_BLACK = 3,
} ChiColor;

/**
 * An object allocated on the major heap consists
 * of an atomic header word and the payload. The header
 * word bitfield is configured by the _CHI_BF_* defines.
 * Unused objects are kept in singly-linked lists, where
 * the header word is reused as pointer to the next unused object.
 */
struct ChiObject_ {
    ChiAtomicWord header;
    union {
        ChiObject* next;
        ChiWord    payload[0];
    };
};

CHI_INL bool chiMutableType(ChiType t) {
    return t >= CHI_FIRST_MUTABLE && t <= CHI_LAST_MUTABLE;
}

CHI_INL bool chiMutable(Chili c) {
    return chiMutableType(chiType(c));
}

CHI_WU const char* chiTypeName(ChiType);

CHI_INL CHI_WU bool chiMajor(Chili c) {
    // Right now, it holds major <=> pinned.
    // We keep it separate for clarity and future changes.
    return chiPinned(c);
}

/**
 * Returns heap generation of object
 * @arg mask Address mask to retrieve block start
 */
CHI_INL ChiGen chiGen(Chili c, uintptr_t mask) {
    /* TODO: Investigate if we should store the generation differently.
     * Right now the generation is stored within the block header.
     * Alternatively we could store the generation in the block address.
     * Another alternative would be to store the generation directly in the Chili word.
     */
    return chiMajor(c) ? CHI_GEN_MAJOR : chiBlock(chiRawPayload(c), mask)->gen;
}

CHI_INL CHI_WU uintptr_t chiAddress(Chili c) {
    CHI_ASSERT(!chiUnboxed(c));
    return CHI_BF_GET(_CHILI_BF_PTR, CHILI_UN(c));
}

// Object init does not imply a memory barrier!
CHI_INL void chiObjectInit(ChiObject* obj, size_t fullSize, ChiColor color) {
    CHI_ASSERT(fullSize >= _CHI_HEADER_OVERHEAD);
    size_t size = fullSize - _CHI_HEADER_OVERHEAD;
    atomic_init(&obj->header, CHI_BF_INIT(_CHI_OBJECT_BF_SIZE, size)
                | CHI_BF_INIT(_CHI_OBJECT_BF_COLOR, color));
    chiPoisonObject(obj->payload, size);
}

CHI_INL CHI_WU ChiObject* chiObject(Chili c) {
    CHI_ASSERT(chiMajor(c));
    return CHI_ALIGN_CAST32((ChiWord*)chiRawPayload(c) - 1, ChiObject*);
}

CHI_INL CHI_WU bool chiObjectDirty(const ChiObject* o) {
    return CHI_BF_GET(_CHI_OBJECT_BF_DIRTY, o->header);
}

CHI_INL CHI_WU bool chiObjectCasDirty(ChiObject* o, bool old, bool set) {
    return CHI_BF_ATOMIC_CAS(_CHI_OBJECT_BF_DIRTY, o->header, old, set);
}

CHI_INL void chiObjectSetDirty(ChiObject* o, bool set) {
    CHI_BF_ATOMIC_SET(_CHI_OBJECT_BF_DIRTY, o->header, set);
}

CHI_INL CHI_WU bool chiObjectActive(const ChiObject* o) {
    return CHI_BF_GET(_CHI_OBJECT_BF_ACTIVE, o->header);
}

CHI_INL CHI_WU bool chiObjectCasActive(ChiObject* o, bool old, bool set) {
    return CHI_BF_ATOMIC_CAS(_CHI_OBJECT_BF_ACTIVE, o->header, old, set);
}

CHI_INL CHI_WU ChiColor chiObjectColor(const ChiObject* o) {
    return CHI_BF_GET(_CHI_OBJECT_BF_COLOR, o->header);
}

CHI_INL CHI_WU bool chiObjectCasColor(ChiObject* o, ChiColor old, ChiColor set) {
    return CHI_BF_ATOMIC_CAS(_CHI_OBJECT_BF_COLOR, o->header, old, set);
}

CHI_INL void chiObjectSetColor(ChiObject* o, ChiColor c) {
    CHI_BF_ATOMIC_SET(_CHI_OBJECT_BF_COLOR, o->header, c);
}

CHI_INL CHI_WU size_t chiObjectSize(const ChiObject* o) {
    return CHI_BF_GET(_CHI_OBJECT_BF_SIZE, o->header);
}

CHI_INL void chiObjectSetSize(ChiObject* o, size_t size) {
    CHI_BF_ATOMIC_SET(_CHI_OBJECT_BF_SIZE, o->header, size);
}

CHI_INL void chiObjectShrink(ChiObject* o, size_t oldSize, size_t newSize) {
    CHI_ASSERT(oldSize == chiObjectSize(o));
    CHI_ASSERT(newSize < oldSize);
    ChiObject *rest = CHI_ALIGN_CAST32(o->payload + newSize, ChiObject*);
    chiObjectInit(rest, oldSize - newSize, CHI_WHITE);
    chiObjectSetSize(o, newSize); // implies memory barrier, must happen after rest initialization
}
