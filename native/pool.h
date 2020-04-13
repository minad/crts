#pragma once

#include "private.h"

typedef struct ChiPoolObject_ ChiPoolObject;
struct ChiPoolObject_ { ChiPoolObject* next; };

typedef struct {
    size_t object;
    void *next, *end;
    ChiPoolObject* free;
} ChiPool;

CHI_INL ChiPoolObject* _chiPoolAlloc(ChiPool* pool) {
    CHI_ASSERT((uint8_t*)pool->next + pool->object <= (uint8_t*)pool->end);
    void* obj = pool->next;
    pool->next = (uint8_t*)obj + pool->object;
    return (ChiPoolObject*)obj;
}

CHI_INL void* _chiPoolGet(ChiPool* pool) {
    ChiPoolObject* obj = pool->free;
    pool->free = obj->next;
    return obj;
}

CHI_INL void chiPoolPut(ChiPool* pool, void* obj) {
    ChiPoolObject* po = (ChiPoolObject*)obj;
    po->next = pool->free;
    pool->free = po;
}

CHI_INL void chiPoolGrow(ChiPool* pool, void* start, size_t size) {
    while ((uint8_t*)pool->next + pool->object <= (uint8_t*)pool->end)
        chiPoolPut(pool, _chiPoolAlloc(pool));
    pool->next = start;
    pool->end = (uint8_t*)start + size;
}

CHI_INL void* chiPoolTryGet(ChiPool* pool) {
    return CHI_LIKELY(pool->free) ? _chiPoolGet(pool)
        : (uint8_t*)pool->next + pool->object <= (uint8_t*)pool->end
        ? _chiPoolAlloc(pool) : 0;
}

CHI_INL void* chiPoolGet(ChiPool* pool) {
    return CHI_LIKELY(pool->free) ? _chiPoolGet(pool) : _chiPoolAlloc(pool);
}

CHI_INL bool chiPoolAvail(ChiPool* pool, size_t n) {
    size_t avail = (size_t)((uint8_t*)pool->end - (uint8_t*)pool->next) / pool->object;
    if (n <= avail)
        return true;
    n -= avail;
    ChiPoolObject* obj = pool->free;
    while (obj && n --> 0)
        obj = obj->next;
    return !!obj;
}

CHI_INL void chiPoolInit(ChiPool* pool, size_t object) {
    CHI_ZERO_STRUCT(pool);
    pool->object = object;
}
