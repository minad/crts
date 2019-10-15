#pragma once

#include "object.h"

typedef struct ChiProcessor_ ChiProcessor;

CHI_INTERN CHI_WU Chili chiThreadNewUninitialized(ChiProcessor*);
CHI_INTERN void chiThreadSetErrno(ChiProcessor*, int32_t);
CHI_INTERN void chiThreadSetInterruptible(ChiProcessor*, bool);
CHI_INTERN CHI_WU Chili chiThreadName(Chili);
CHI_INTERN CHI_WU bool chiThreadInterruptible(Chili);
CHI_INTERN CHI_WU uint32_t chiThreadId(Chili);

/// Use chiThreadStack/chiThreadStackInactive instead, which checks the activation status
CHI_INL CHI_WU Chili chiThreadStackUnchecked(Chili c) {
    return chiFieldRead(&chiToThread(c)->stack);
}

CHI_INL CHI_WU Chili chiThreadStackInactive(Chili c) {
    Chili stack = chiThreadStackUnchecked(c);
    CHI_ASSERT(!chiObjectLocked(chiObject(stack)));
    return stack;
}

CHI_INL CHI_WU Chili chiThreadStack(Chili c) {
    Chili stack = chiThreadStackUnchecked(c);
    CHI_ASSERT(chiObjectLocked(chiObject(stack)));
    CHI_ASSERT(!chiObjectShared(chiObject(stack)));
    return stack;
}
