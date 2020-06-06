#pragma once

#include "grayvec.h"
#include "heap.h"

typedef struct {
    ChiGrayVec          gray;
    _Atomic(ChiGCPhase) phase;
    ChiMarkState        markState;
} ChiLocalGC;

typedef struct ChiGC_ ChiGC;

CHI_INTERN void chiLocalGCSetup(ChiLocalGC*, ChiGC*);
CHI_INTERN void chiLocalGCDestroy(ChiLocalGC*, ChiGC*);
CHI_INTERN void chiMarkBarrier(ChiLocalGC*, Chili, Chili);

CHI_INL void chiMarkNormal(ChiLocalGC* gc, Chili c) {
    CHI_ASSERT(!chiRaw(chiType(c)));
    ChiObject* obj = chiObject(c);
    ChiMarkState ms = gc->markState;
    if (chiColorEq(ms.white, chiObjectColor(obj))) {
        chiObjectSetColor(obj, ms.gray);
        chiTrigger(&gc->gray.null, false);
        chiObjVecPush(&gc->gray.vec, c);
    }
}

CHI_INL void chiMarkRaw(ChiLocalGC* gc, Chili c) {
    CHI_ASSERT(chiRaw(chiType(c)));
    ChiObject* obj = chiObject(c);
    ChiMarkState ms = gc->markState;
    CHI_ASSERT(!chiColorEq(chiObjectColor(obj), ms.gray));
    chiObjectSetColor(obj, ms.black);
}

CHI_INL void chiMarkRef(ChiLocalGC* gc, Chili c) {
    CHI_ASSERT(chiRefMajor(c));
    if (chiRaw(chiType(c)))
        chiMarkRaw(gc, c);
    else
        chiMarkNormal(gc, c);
}

CHI_INL void chiMark(ChiLocalGC* gc, Chili c) {
    if (chiRefMajor(c))
        chiMarkRef(gc, c);
}
