#pragma once

#include "objvec.h"

typedef struct {
    ChiObjVec vec;
    ChiTrigger null;
} ChiGrayVec;

CHI_INL void chiGrayVecInit(ChiGrayVec* gray, ChiBlockManager* bm) {
    chiTrigger(&gray->null, true);
    chiObjVecInit(&gray->vec, bm);
}

CHI_INL void chiGrayVecFree(ChiGrayVec* gray) {
    chiObjVecFree(&gray->vec);
}

CHI_INL void chiGrayVecUpdateNull(ChiGrayVec* gray) {
    chiTrigger(&gray->null, chiObjVecNull(&gray->vec));
}

CHI_INL void chiGrayVecJoin(ChiGrayVec* dst, ChiGrayVec* src) {
    chiTrigger(&dst->null, false);
    chiObjVecJoin(&dst->vec, &src->vec);
    chiGrayVecUpdateNull(dst);
    chiTrigger(&src->null, true);
}

CHI_INL bool chiGrayVecNull(ChiGrayVec* gray) {
    return chiTriggered(&gray->null);
}

CHI_INL void chiGrayVecTransfer(ChiGrayVec* dst, ChiGrayVec* src) {
    chiTrigger(&dst->null, false);
    chiObjVecTransfer(&dst->vec, &src->vec);
    chiGrayVecUpdateNull(src);
}
