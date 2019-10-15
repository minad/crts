#pragma once

#include "private.h"

typedef struct ChiPoolObject_ ChiPoolObject;
struct ChiPoolObject_ { ChiPoolObject* next; };

typedef struct {
    size_t object;
    void *next, *end;
    ChiPoolObject* free;
} ChiPool;

CHI_INTERN void chiPoolGrow(ChiPool*, void*, size_t);
CHI_INTERN void* chiPoolGet(ChiPool*);
CHI_INTERN void chiPoolPut(ChiPool*, void*);
CHI_INTERN void chiPoolInit(ChiPool*, size_t);
CHI_INTERN bool chiPoolAvail(ChiPool*, size_t);
