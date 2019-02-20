#include "adt.h"
#include "barrier.h"
#include "event.h"
#include "location.h"
#include "sink.h"
#include "runtime.h"
#include "stack.h"
#include "thread.h"
#include "asm.h"
#include <chili/object/array.h>
#include <chili/object/thunk.h>

CHI_COLD static uint32_t stackDepth(const ChiStack* stack, uint32_t max) {
    uint32_t depth = 0;
    for (const Chili* p = stack->sp - 1; p >= stack->base && depth < max; p -= chiFrameSize(p))
        ++depth;
    return depth;
}

CHI_COLD static Chili stackTraceElement(const ChiLocInfo* loc) {
    return chiNewTuple(chiStringNew(loc->file),
                         chiStringNew(loc->fn),
                         chiFromUnboxed(loc->line),
                         chiStringNew(loc->module));
}

Chili chiStackNew(ChiProcessor* proc) {
    // Stacks are always pinned for better cpu caching, since they are heavily mutated.
    // Furthermore we don't have to modify the stack pointer during gc when the stack would be moved.
    const ChiRuntimeOption* opt = &proc->rt->option;
    Chili c = chiNewPin(CHI_STACK, opt->stack.init);
    chiStackInitCanary(c);
    ChiStack* stack = chiToStack(c);
    stack->sp = stack->base;
    return c;
}

static size_t computeStackSize(ChiProcessor* proc, size_t oldSize, size_t reqSize) {
    const ChiRuntimeOption* opt = &proc->rt->option;

    // Always resize
    //return reqSize;

    // Always shrink
    //if (reqSize < oldSize)
    //    return reqSize;

    size_t newSize = oldSize;
    if (reqSize > newSize) {
        while (reqSize > newSize)
            newSize = opt->stack.growth * newSize / 100;
    } else {
        while (reqSize < 100 * newSize / opt->stack.growth &&
               newSize - reqSize > 10 * opt->stack.init &&
               newSize > opt->stack.init)
            newSize = 100 * newSize / opt->stack.growth;
    }

    return newSize;
}

Chili chiStackTryResize(ChiProcessor* proc, Chili* sp, Chili* newLimit) {
    Chili oldStackObj = chiThreadStack(proc->thread);
    ChiStack* oldStack = chiToStack(oldStackObj);
    oldStack->sp = sp;

    const size_t oldSize = chiSize(oldStackObj);
    const size_t usedSize = (size_t)(oldStack->sp + CHI_STACK_CONT_SIZE + CHI_STACK_CANARY_SIZE - (const Chili*)oldStack);
    const size_t reqSize = (size_t)(newLimit + CHI_STACK_CONT_SIZE + CHI_STACK_CANARY_SIZE - (const Chili*)oldStack);
    CHI_ASSERT(usedSize <= oldSize);
    CHI_ASSERT(usedSize <= reqSize);

    size_t newSize = computeStackSize(proc, oldSize, reqSize);
    if (newSize == oldSize)
        return oldStackObj;

    if (newSize < oldSize) {
        CHI_EVENT(proc, STACK_SHRINK,
                  .oldSize = oldSize,
                  .newSize = newSize,
                  .reqSize = reqSize,
                  .usedSize = usedSize);
        chiObjectShrink(chiObject(oldStackObj), oldSize, newSize);
        chiStackInitCanary(oldStackObj);
        return oldStackObj;
    }

    CHI_EVENT(proc, STACK_GROW,
              .oldSize = oldSize,
              .newSize = newSize,
              .reqSize = reqSize,
              .usedSize = usedSize);

    Chili newStackObj = chiNewFlags(CHI_STACK, newSize, CHI_NEW_PIN | CHI_NEW_TRY);
    if (!chiSuccess(newStackObj))
        return CHI_FAIL;

    chiStackInitCanary(newStackObj);
    ChiStack* newStack = chiToStack(newStackObj);
    newStack->sp = newStack->base + (oldStack->sp - oldStack->base);
    memcpy(newStack->base, oldStack->base, CHI_WORDSIZE * (size_t)(oldStack->sp - oldStack->base + CHI_STACK_CONT_SIZE));
    return newStackObj;
}

CHI_COLD void chiStackDump(ChiSink* sink, ChiProcessor* proc) {
    chiSinkFmt(sink, "[%s]", proc->worker.name);

    Chili name = chiThreadName(proc->thread);
    ChiWord tid = chiToUnboxed(chiToThread(proc->thread)->tid);
    if (chiTrue(name))
        chiSinkFmt(sink, ";[thread %ju %S]", tid, chiStringRef(&name));
    else
        chiSinkFmt(sink, ";[unnamed thread %ju]", tid);

    const ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    const Chili* p = stack->sp - 1, *last = 0;
    const uint32_t count = proc->rt->option.stack.trace;
    for (uint32_t n = 0; n < count && p >= stack->base; ++n, p -= chiFrameSize(p)) {
        if (!chiFrameIdentical(p, last)) {
            ChiLocLookup lookup;
            chiLocLookup(&lookup, chiLocateFrame(p));
            chiSinkFmt(sink, ";%L", &lookup.loc);
            last = p;
        }
    }
}

CHI_COLD Chili chiStackGetTrace(ChiProcessor* proc, Chili* sp) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    if (!opt->stack.trace)
        return CHI_FALSE;

    ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    stack->sp = sp; // update stack pointer, before stack walking

    uint32_t depth = stackDepth(stack, opt->stack.trace);
    if (!depth)
        return CHI_FALSE;

    Chili trace = chiArrayNewFlags(depth, CHI_FALSE, CHI_NEW_UNINITIALIZED | CHI_NEW_TRY);
    if (!chiSuccess(trace))
        return CHI_FALSE;

    const Chili* p = stack->sp - 1;
    for (uint32_t n = 0; n < depth && p >= stack->base; ++n, p -= chiFrameSize(p)) {
        ChiLocLookup lookup;
        chiLocLookup(&lookup, chiLocateFrame(p));
        chiAtomicInit(chiArrayField(trace, n), stackTraceElement(&lookup.loc));
    }

    return chiNewJust(trace);
}

Chili* chiStackUnwind(ChiProcessor* proc, Chili except) {
    Chili thrower = CHI_FALSE, clos;
    const Chili bh = proc->rt->blackhole;
    const ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    for (Chili* p = stack->sp - 1; p >= stack->base; p -= chiFrameSize(p)) {
        ChiFrame t = chiFrame(p);
        if (t == CHI_FRAME_CATCH)
            return p - 1;
        if (t == CHI_FRAME_UPDATE && !chiThunkForced(p[-1], &clos) && chiIdentical(clos, bh)) {
            if (!chiTrue(thrower))
                thrower = chiNewVariadic(CHI_THUNKFN, chiFromCont(&chiThunkThrow), except);
            chiThunkCas(proc, p[-1], bh, thrower);
        }
    }
    return 0;
}

void chiStackBlackhole(ChiProcessor* proc) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    if (opt->blackhole == CHI_BH_NONE)
        return;
    const ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    size_t count = 0;
    for (const Chili* p = stack->sp - 1; p >= stack->base; p -= chiFrameSize(p)) {
        if (chiFrame(p) == CHI_FRAME_UPDATE) {
            Chili thunk = p[-1], clos;
            if (!chiThunkForced(thunk, &clos) && chiThunkCas(proc, thunk, clos, proc->rt->blackhole))
                ++count;
        }
    }
    if (count != 0)
        CHI_EVENT(proc, STACK_BLACKHOLE, .count = count);
}

void chiStackActivate(ChiProcessor* proc) {
    ChiObject* obj = chiObject(chiThreadStackUnchecked(proc->thread));
    // Busy looping until stack is available
    CHI_BUSY_WHILE(!chiObjectCasActive(obj, false, true));
}

void chiStackDeactivate(ChiProcessor* proc, Chili* sp) {
    Chili stack = chiThreadStackUnchecked(proc->thread);
    chiToStack(stack)->sp = sp;
    ChiObject* obj = chiObject(stack);
    bool wasActive = chiObjectCasActive(obj, true, false);
    CHI_ASSERT(wasActive);

    const ChiRuntimeOption* opt = &proc->rt->option;
    if (CHI_UNLIKELY(opt->gc.mode == CHI_GC_NONE))
        return;

    if (chiObjectCasDirty(obj, false, true))
        chiBlockVecPush(proc->gcPS.dirty, stack);

    // Steele barrier
    if (proc->rt->gc.msPhase == CHI_MS_PHASE_MARK
        && chiObjectCasColor(obj, CHI_BLACK, CHI_GRAY))
        chiBlockVecPush(&proc->gcPS.grayList, stack);
}

void chiStackDeactivateReset(ChiProcessor* proc) {
    Chili stack = chiThreadStackUnchecked(proc->thread);
    ChiStack* s = chiToStack(stack);
    s->sp = s->base;
    bool wasActive = chiObjectCasActive(chiObject(stack), true, false);
    CHI_ASSERT(wasActive);
}

bool chiBlackholeEvaporated(Chili bh) {
    return !chiIdentical(bh, CHI_CURRENT_RUNTIME->blackhole);
}
