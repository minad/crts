#pragma once

#include "localheap.h"

CHI_INTERN void chiPromoteLocal(ChiProcessor*, Chili*);
CHI_INTERN void chiPromoteShared(ChiProcessor*, Chili*);

#if CHI_SYSTEM_HAS_TASK
typedef struct {
    CHI_IF(CHI_COUNT_ENABLED, ChiPromoteStats stats;)
    ChiProcessor*   proc;
    ChiBlockVec     stack;
    uintptr_t       blockMask;
    uint32_t        depth;
    bool            forward;
} ChiPromoteState;

CHI_INTERN void chiPromoteSharedBegin(ChiPromoteState*, ChiProcessor*);
CHI_INTERN void chiPromoteSharedEnd(ChiPromoteState*);
CHI_INTERN void chiPromoteSharedObject(ChiPromoteState*, Chili*);
#else
typedef ChiProcessor* ChiPromoteState;
CHI_INL void chiPromoteSharedBegin(ChiPromoteState* state, ChiProcessor* proc) { *state = proc; }
CHI_INL void chiPromoteSharedEnd(ChiPromoteState* state) { *state = 0; }
CHI_INL void chiPromoteSharedObject(ChiPromoteState* state, Chili* cp) { chiPromoteLocal(*state, cp); }
#endif
