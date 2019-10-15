#pragma once

#include "blockalloc.h"
#include "blockvec.h"
#include "heap.h"

typedef enum {
    CHI_BARRIER_NONE,
    CHI_BARRIER_SYNC,
    CHI_BARRIER_ASYNC,
} ChiBarrier;

typedef struct {
    ChiBlockVec vec;
    ChiTrigger null;
} ChiGrayVec;

typedef struct {
    ChiGrayVec           gray;
    _Atomic(ChiGCPhase)  phase;
    ChiBarrier           barrier;
    ChiColorState        colorState;
} ChiLocalGC;

typedef struct ChiGC_ ChiGC;

CHI_INTERN void chiLocalGCSetup(ChiLocalGC*, ChiGC*);
CHI_INTERN void chiLocalGCDestroy(ChiLocalGC*, ChiGC*);

CHI_INL void chiGrayInit(ChiGrayVec* gray, ChiBlockManager* bm) {
    chiTrigger(&gray->null, true);
    chiBlockVecInit(&gray->vec, bm);
}

CHI_INL void chiGrayFree(ChiGrayVec* gray) {
    chiBlockVecFree(&gray->vec);
}

CHI_INL void chiGrayMark(ChiLocalGC* gc, Chili c) {
    CHI_ASSERT(chiType(c) != CHI_STACK);
    ChiObject* obj = chiObject(c);
    ChiColor color = chiObjectColor(obj);
    if (chiRaw(chiType(c))) {
        CHI_ASSERT(!chiColorEq(color, gc->colorState.gray));
        chiObjectSetColor(obj, gc->colorState.black);
    } else if (chiColorEq(gc->colorState.white, color)) {
        chiObjectSetColor(obj, gc->colorState.gray);
        chiTrigger(&gc->gray.null, false);
        chiBlockVecPush(&gc->gray.vec, c);
    }
}

CHI_INL void chiGrayMarkUnboxed(ChiLocalGC* gc, Chili c) {
    if (!chiUnboxed(c) && chiGen(c) == CHI_GEN_MAJOR)
        chiGrayMark(gc, c);
}

CHI_INL void chiGrayUpdateNull(ChiGrayVec* gray) {
    chiTrigger(&gray->null, chiBlockVecNull(&gray->vec));
}

CHI_INL void chiGrayJoin(ChiGrayVec* dst, ChiGrayVec* src) {
    chiTrigger(&dst->null, false);
    chiBlockVecJoin(&dst->vec, &src->vec);
    chiGrayUpdateNull(dst);
    chiGrayUpdateNull(src);
}

CHI_INL bool chiGrayNull(ChiGrayVec* gray) {
    return chiTriggered(&gray->null);
}

CHI_INL void chiGrayTake(ChiGrayVec* dst, ChiGrayVec* src) {
    CHI_ASSERT(dst != src);
    CHI_ASSERT(dst->vec.manager == src->vec.manager);
    CHI_ASSERT(!chiGrayNull(src));
    chiTrigger(&dst->null, false);
    chiBlockListAppend(&dst->vec.list, chiBlockListPop(&src->vec.list));
    chiGrayUpdateNull(src);
}
