#pragma once

#include "private.h"

typedef struct ChiPoolObject_ ChiPoolObject;
struct ChiPoolObject_ { ChiPoolObject* next; };

typedef struct {
    size_t avail, object;
    void* next;
    void* end;
    ChiPoolObject* free;
} ChiPool;

void chiPoolGrow(ChiPool*, void*, size_t);
void* chiPoolGet(ChiPool*);
void chiPoolPut(ChiPool*, void*);
void chiPoolInit(ChiPool*, size_t);
