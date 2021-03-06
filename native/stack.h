#pragma once

#include "system.h"
#include "thread.h"

enum {
    CHI_STACK_OVERHEAD = 1 + CHI_POISON_STACK_CANARY_SIZE, // header + canary
};

CHI_OBJECT(STACK, Stack, Chili* sp; CHI_IF(CHI_ARCH_32BIT, uint32_t _pad;) Chili base[])

typedef struct ChiProcessor_ ChiProcessor;
typedef struct ChiSink_ ChiSink;

CHI_INTERN CHI_WU Chili chiStackNew(ChiProcessor*, size_t);
CHI_INTERN CHI_WU uint32_t chiStackTryGrow(ChiProcessor*, Chili*, Chili*);
CHI_INTERN CHI_WU bool chiStackUnwind(ChiProcessor*, Chili*);
CHI_INTERN void chiStackDump(ChiSink*, ChiProcessor*, Chili*);
CHI_INTERN void chiStackActivate(ChiProcessor*);
CHI_INTERN void chiStackDeactivate(ChiProcessor*, Chili*);
CHI_INTERN void chiStackShrink(ChiProcessor*);
CHI_INTERN CHI_WU Chili chiStackGetTrace(ChiProcessor*, Chili*);
CHI_INTERN_CONT_DECL(chiStackGrowCont)

CHI_INL Chili* chiStackLimit(Chili c) {
    CHI_ASSERT(chiType(c) == CHI_STACK);
    ChiObject* obj = chiObject(c);
    return (Chili*)obj->payload + obj->size - CHI_POISON_STACK_CANARY_SIZE;
}

CHI_INL void chiStackCheckCanary(const Chili* sl) {
    if (CHI_POISON_STACK_CANARY_SIZE) {
        for (uint32_t i = 0; i < CHI_MAX(1U, CHI_POISON_STACK_CANARY_SIZE); ++i, ++sl)
            CHI_ASSERT_POISONED(CHILI_UN(*sl), CHI_POISON_STACK_CANARY);
    }
}

CHI_INL CHI_WU ChiContInfo* chiFrameInfo(const Chili* p) {
    return chiContInfo(chiToCont(*p));
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
    if (CHI_AND(CHI_CBY_SUPPORT_ENABLED, chiFrameInfo(p)->interp && chiFrameInfo(q)->interp)) {
        p = p - 3;
        q = q - 3;
    }
    return chiIdentical(*p, *q);
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
    bool _cycles, incomplete;
} ChiStackWalk;

CHI_INL ChiStackWalk chiStackWalkInit(Chili stack, Chili* sp, uint32_t max, bool cycles) {
    return (ChiStackWalk){ ._base = chiToStack(stack)->base, .frame = sp - 1, ._max = max, ._cycles = cycles };
}

CHI_INL bool chiStackWalk(ChiStackWalk* w) {
    CHI_ASSERT(w->depth <= w->_max);
    CHI_ASSERT(w->frame >= w->_base - 1);
    if (w->depth == w->_max) {
        w->incomplete = true;
        return false;
    }
    if (w->frame < w->_base)
        return false;
    if (w->_last) {
        if (CHI_UNLIKELY(chiToCont(*w->frame) == &chiStackGrowCont)) {
            /* We continue to walk the next stack segment
             * Note that the stack segment is NOT locked!
             * However since we hold the lock on the topmost segment,
             * we can be sure that the frame structure of the
             * stack is not modified under our hands.
             * However stack frame fields might be rewritten by the marker thread.
             */
            CHI_ASSERT(w->frame == w->_base + 1);
            ChiStack* prev = chiToStack(*w->_base);
            w->_base = prev->base;
            w->frame = prev->sp - 1;
        } else {
            do {
                w->frame -= chiFrameSize(w->frame);
                if (w->frame < w->_base) {
                    CHI_ASSERT(w->frame == w->_base - 1);
                    return false;
                }
            } while (!w->_cycles && chiFrameIdentical(w->frame, w->_last));
        }
    }
    w->_last = w->frame;
    ++w->depth;
    return true;
}

CHI_INL void chiStackDebugWalk(Chili stack, const Chili* sp, const Chili* sl) {
    chiStackCheckCanary(sl);
#if CHI_STACK_DEBUG_WALK == 0
    CHI_NOWARN_UNUSED(stack);
    CHI_NOWARN_UNUSED(sp);
#else
    const Chili *base = chiToStack(stack)->base, *p = sp - 1;
    for (size_t depth = 0;
         p >= base && (CHI_STACK_DEBUG_WALK < 0 || depth < (size_t)CHI_STACK_DEBUG_WALK);
         ++depth, p -= chiFrameSize(p));
    CHI_ASSERT(CHI_STACK_DEBUG_WALK >= 0 || p == base - 1);
#endif
}
