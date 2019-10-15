#include "pool.h"

static ChiPoolObject* poolAlloc(ChiPool* pool) {
    if ((uint8_t*)pool->next + pool->object > (uint8_t*)pool->end)
        return 0;
    void* obj = pool->next;
    pool->next = (uint8_t*)obj + pool->object;
    return (ChiPoolObject*)obj;
}

void chiPoolPut(ChiPool* pool, void* obj) {
    ChiPoolObject* po = (ChiPoolObject*)obj;
    po->next = pool->free;
    pool->free = po;
}

void chiPoolGrow(ChiPool* pool, void* start, size_t size) {
    while ((uint8_t*)pool->next + pool->object <= (uint8_t*)pool->end)
        chiPoolPut(pool, poolAlloc(pool));
    pool->next = start;
    pool->end = (uint8_t*)start + size;
}

void* chiPoolGet(ChiPool* pool) {
    if (CHI_UNLIKELY(!pool->free))
        return poolAlloc(pool);
    ChiPoolObject* obj = pool->free;
    pool->free = obj->next;
    return obj;
}

bool chiPoolAvail(ChiPool* pool, size_t n) {
    size_t avail = (size_t)((uint8_t*)pool->end - (uint8_t*)pool->next) / pool->object;
    if (n <= avail)
        return true;
    n -= avail;
    ChiPoolObject* obj = pool->free;
    while (obj && n --> 0)
        obj = obj->next;
    return !!obj;
}

void chiPoolInit(ChiPool* pool, size_t object) {
    CHI_ZERO_STRUCT(pool);
    pool->object = object;
}
