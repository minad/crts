#pragma once

#include "system.h"
#include "thread.h"

enum {
    CHI_STACK_CONT_SIZE = 1,
    CHI_STACK_OVERHEAD = 1 + CHI_POISON_STACK_CANARY_SIZE, // header + canary
};

CHI_OBJECT(STACK, Stack, Chili* sp; CHI_IF(CHI_ARCH_32BIT, uint32_t _pad;) Chili base[])

typedef struct ChiProcessor_ ChiProcessor;
typedef struct ChiSink_ ChiSink;

CHI_INTERN CHI_WU Chili chiStackNew(ChiProcessor*);
CHI_INTERN CHI_WU Chili chiStackTryResize(ChiProcessor*, Chili*, Chili*);
CHI_INTERN CHI_WU Chili* chiStackUnwind(ChiProcessor*, Chili*);
CHI_INTERN void chiStackDump(ChiSink*, ChiProcessor*, Chili*);
CHI_INTERN void chiStackActivate(ChiProcessor*);
CHI_INTERN void chiStackDeactivate(ChiProcessor*, Chili*);
CHI_INTERN CHI_WU Chili chiStackGetTrace(ChiProcessor*, Chili*);

CHI_INL Chili* chiStackLimit(Chili c) {
    CHI_ASSERT(chiType(c) == CHI_STACK);
    return chiPayload(c) + chiSize(c) - CHI_POISON_STACK_CANARY_SIZE - CHI_STACK_CONT_SIZE;
}

CHI_INL void chiStackCheckCanary(const Chili* sl) {
    if (CHI_POISON_STACK_CANARY_SIZE) {
        sl += CHI_STACK_CONT_SIZE;
        for (uint32_t i = 0; i < CHI_MAX(1U, CHI_POISON_STACK_CANARY_SIZE); ++i, ++sl)
            CHI_ASSERT_POISONED(CHILI_UN(*sl), CHI_POISON_STACK_CANARY);
    }
}

CHI_INL CHI_WU ChiContInfo* chiContInfo(ChiCont cont) {
#if CHI_CONT_PREFIX
    return CHI_ALIGN_CAST((uint8_t*)cont - CHI_CONT_PREFIX_ALIGN, ChiContInfo*);
#else
    return cont;
#endif
}

CHI_INL CHI_WU ChiContInfo* chiFrameInfo(const Chili* p) {
    return chiContInfo(chiToCont(*p));
}

CHI_INL CHI_WU ChiFrame chiFrame(const Chili* p) {
    return chiFrameInfo(p)->type;
}

CHI_INL CHI_WU uint32_t chiFrameSize(const Chili* p) {
    uint32_t size = chiFrameInfo(p)->size;
    if (size == CHI_VASIZE)
        size = chiToUInt32(p[-1]);
    CHI_ASSERT(size >= 1);
    return size;
}

CHI_INL CHI_WU bool chiFrameIdentical(const Chili* p, const Chili* q) {
    CHI_ASSERT(p);
    CHI_ASSERT(q);
    if (CHI_AND(CHI_CBY_SUPPORT_ENABLED, chiFrame(p) == CHI_FRAME_INTERP) && chiFrame(p) == chiFrame(q)) {
        p = p - 3;
        q = q - 3;
    }
    return chiIdentical(*p, *q);
}

CHI_INL CHI_WU uint32_t chiContArgs(ChiCont c, Chili regA0, uint8_t regAPPN) {
    ChiContInfo* info = chiContInfo(c);
    uint8_t na = info->na;
    if (na == CHI_VAARGS) {
        if (info->type == CHI_FRAME_APPN)
            return regAPPN;
        return chiFnOrThunkArity(regA0);
    }
    return na;
}

/**
 * Simple stack walker with depth limitation
 * and filtering of 1-cycles.
 * This walker is useful for stack traces, profiling etc.
 * It is not appropriate for full unfiltered stack
 * walking during unwinding.
 */
typedef struct {
    // Underscored fields are private
    Chili *frame, *_base, *_last;
    uint32_t depth, _max;
    bool _cycles;
} ChiStackWalk;

CHI_INL ChiStackWalk chiStackWalkInit(ChiStack* stack, Chili* sp, uint32_t max, bool cycles) {
    return (ChiStackWalk){ ._base = stack->base, .frame = sp - 1, ._max = max, ._cycles = cycles };
}

CHI_INL bool chiStackWalk(ChiStackWalk* w) {
    CHI_ASSERT(w->depth <= w->_max);
    CHI_ASSERT(w->frame >= w->_base - 1);
    if (w->depth == w->_max || w->frame < w->_base)
        return false;
    if (w->_last) {
        do {
            w->frame -= chiFrameSize(w->frame);
            if (w->frame < w->_base) {
                CHI_ASSERT(w->frame == w->_base - 1);
                return false;
            }
        } while (!w->_cycles && chiFrameIdentical(w->frame, w->_last));
    }
    w->_last = w->frame;
    ++w->depth;
    return true;
}

CHI_INL void chiStackDebugWalk(Chili thread, const Chili* sp, const Chili* sl) {
    chiStackCheckCanary(sl);
    if (CHI_STACK_DEBUG_WALK) {
        const Chili *base = chiToStack(chiThreadStack(thread))->base, *p = sp - 1;
        for (; p >= base; p -= chiFrameSize(p));
        CHI_ASSERT(p == base - 1);
    }
}
