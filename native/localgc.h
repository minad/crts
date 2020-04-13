#pragma once

#include "blockalloc.h"
#include "blockvec.h"
#include "heap.h"

typedef struct {
    ChiBlockVec vec;
    ChiTrigger null;
} ChiGrayVec;

typedef struct {
    ChiGrayVec          gray;
    _Atomic(ChiGCPhase) phase;
    ChiMarkState        markState;
} ChiLocalGC;

typedef struct ChiGC_ ChiGC;

CHI_INTERN void chiLocalGCSetup(ChiLocalGC*, ChiGC*);
CHI_INTERN void chiLocalGCDestroy(ChiLocalGC*, ChiGC*);
CHI_INTERN void chiGrayMarkBarrier(ChiLocalGC*, Chili, Chili);

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
    ChiMarkState ms = gc->markState;
    if (chiRaw(chiType(c))) {
        CHI_ASSERT(!chiColorEq(chiObjectColor(obj), ms.gray));
        chiObjectSetColor(obj, ms.black);
    } else if (chiColorEq(ms.white, chiObjectColor(obj))) {
        chiObjectSetColor(obj, ms.gray);
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

CHI_INL void chiMarkStateInit(ChiMarkState* s) {
    s->white = CHI_WRAP(Color, _CHI_COLOR_USED);
    s->gray  = CHI_WRAP(Color, _CHI_COLOR_SHADED | _CHI_COLOR_USED);
    s->black = CHI_WRAP(Color, _CHI_COLOR_EPOCH + _CHI_COLOR_USED);
}

CHI_INL void chiMarkStateEpoch(ChiMarkState* s) {
    s->black = CHI_WRAP(Color, (uint8_t)(CHI_UN(Color, s->black) + _CHI_COLOR_EPOCH));
    s->gray  = CHI_WRAP(Color, (uint8_t)(CHI_UN(Color, s->gray)  + _CHI_COLOR_EPOCH));
    s->white = CHI_WRAP(Color, (uint8_t)(CHI_UN(Color, s->white) + _CHI_COLOR_EPOCH));
}
