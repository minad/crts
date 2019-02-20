#include "pool.h"

static ChiPoolObject* poolAlloc(ChiPool* pool) {
    CHI_ASSERT((uint8_t*)pool->next + pool->object <= (uint8_t*)pool->end);
    void* obj = pool->next;
    pool->next = (uint8_t*)obj + pool->object;
    return (ChiPoolObject*)obj;
}

static void poolInsert(ChiPool* pool, ChiPoolObject* po) {
    po->next = pool->free;
    pool->free = po;
}

void chiPoolGrow(ChiPool* pool, void* start, size_t size) {
    while ((uint8_t*)pool->next + pool->object <= (uint8_t*)pool->end)
        poolInsert(pool, poolAlloc(pool));
    pool->next = start;
    pool->end = (uint8_t*)start + size;
    pool->avail += size / pool->object;
}

void* chiPoolGet(ChiPool* pool) {
    CHI_ASSERT(pool->avail > 0);
    --pool->avail;
    if (!pool->free)
        return poolAlloc(pool);
    ChiPoolObject* obj = pool->free;
    pool->free = obj->next;
    return obj;
}

void chiPoolPut(ChiPool* pool, void* obj) {
    poolInsert(pool, (ChiPoolObject*)obj);
    ++pool->avail;
}

void chiPoolInit(ChiPool* pool, size_t object) {
    memset(pool, 0, sizeof (ChiPool));
    pool->object = object;
}
