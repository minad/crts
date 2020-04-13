#include "barrier.h"
#include "error.h"
#include "event.h"
#include "location.h"
#include "new.h"
#include "sink.h"
#include "stack.h"
#include "tracepoint.h"

static void stackInitCanary(Chili c) {
    if (CHI_POISON_STACK_CANARY_SIZE) {
        Chili* sl = chiPayload(c) + chiSize(c);
        for (size_t i = 0; i < CHI_MAX(1U, CHI_POISON_STACK_CANARY_SIZE); ++i)
            *(--sl) = (Chili){ CHI_POISON_STACK_CANARY };
    }
}

static Chili stackTraceElement(ChiProcessor* proc, const ChiLocInfo* loc) {
    return chiNewTuple(proc,
                       chiStringNew(proc, loc->file),
                       chiStringNew(proc, loc->fn),
                       chiFromUnboxed(loc->line));
}

Chili chiStackNew(ChiProcessor* proc) {
    // Stacks are always pinned for better cpu caching, since they are heavily mutated.
    // Furthermore we don't have to modify the stack pointer during gc when the stack would be moved.
    const ChiRuntimeOption* opt = &proc->rt->option;
    Chili c = chiNewInl(proc, CHI_STACK, opt->stack.init + CHI_STACK_OVERHEAD, CHI_NEW_LOCAL | CHI_NEW_CLEAN);
    stackInitCanary(c);
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
    CHI_ASSERT(sp <= chiStackLimit(oldStackObj));
    CHI_ASSERT(sp >= oldStack->base);
    oldStack->sp = sp;

    const size_t oldSize = chiSize(oldStackObj) - CHI_STACK_OVERHEAD;
    const size_t usedSize = (size_t)(oldStack->sp + CHI_STACK_CONT_SIZE - oldStack->base);
    const size_t reqSize = (size_t)(newLimit + CHI_STACK_CONT_SIZE - oldStack->base);
    CHI_ASSERT(usedSize <= oldSize);
    CHI_ASSERT(usedSize <= reqSize);

    size_t newSize = computeStackSize(proc, oldSize, reqSize);
    if (newSize == oldSize)
        return oldStackObj;

    Chili newStackObj = chiNewInl(proc, CHI_STACK, newSize + CHI_STACK_OVERHEAD,
                                  CHI_NEW_TRY | CHI_NEW_LOCAL | CHI_NEW_CLEAN);
    if (!chiSuccess(newStackObj)) {
        if (newSize < oldSize)
            return oldStackObj;
        return CHI_FAIL;
    }

    chiEvent(proc, STACK_RESIZE,
             .oldStack = chiAddress(oldStackObj),
             .newStack = chiAddress(newStackObj),
             .oldSize = oldSize,
             .newSize = newSize,
             .reqSize = reqSize,
             .usedSize = usedSize);

    stackInitCanary(newStackObj);
    ChiStack* newStack = chiToStack(newStackObj);
    newStack->sp = newStack->base + (oldStack->sp - oldStack->base);
    memcpy(newStack->base, oldStack->base, CHI_WORDSIZE * usedSize);
    return newStackObj;
}

void chiStackDump(ChiSink* sink, ChiProcessor* proc, Chili* sp) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    ChiStackWalk w = chiStackWalkInit(stack, sp, opt->stack.trace, opt->stack.traceCycles);
    while (chiStackWalk(&w)) {
        ChiLocResolve resolve;
        chiLocResolve(&resolve, chiLocateFrame(w.frame), opt->stack.traceMangled);
        chiSinkFmt(sink, "%L;", &resolve.loc);
    }
    if (w.frame >= stack->base)
        chiSinkPuts(sink, "-;");

    Chili name = chiThreadName(proc->thread);
    uint32_t tid = chiThreadId(proc->thread);
    if (chiTrue(name))
        chiSinkFmt(sink, "[thread %u %S];[%s]", tid, chiStringRef(&name), proc->worker->name);
    else
        chiSinkFmt(sink, "[unnamed thread %u];[%s]", tid, proc->worker->name);
}

static uint32_t stackDepth(ChiStack* stack, Chili* sp, uint32_t max, bool cycles) {
    ChiStackWalk w = chiStackWalkInit(stack, sp, max, cycles);
    while (chiStackWalk(&w));
    return w.depth;
}

Chili chiStackGetTrace(ChiProcessor* proc, Chili* sp) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    if (!opt->stack.trace)
        return CHI_FALSE;

    ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    uint32_t depth = stackDepth(stack, sp, opt->stack.trace, opt->stack.traceCycles);
    if (!depth)
        return CHI_FALSE;

    Chili trace = chiArrayNewUninitialized(proc, depth, CHI_NEW_TRY);
    if (!chiSuccess(trace))
        return CHI_FALSE;

    ChiStackWalk w = chiStackWalkInit(stack, sp, opt->stack.trace, opt->stack.traceCycles);
    while (chiStackWalk(&w)) {
        ChiLocResolve resolve;
        chiLocResolve(&resolve, chiLocateFrame(w.frame), opt->stack.traceMangled);
        chiArrayInit(trace, w.depth - 1, stackTraceElement(proc, &resolve.loc));
    }

    return chiNewJust(proc, trace);
}

Chili* chiStackUnwind(ChiProcessor* proc, Chili* sp) {
    const ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    for (Chili* p = sp - 1; p >= stack->base; p -= chiFrameSize(p)) {
        ChiFrame t = chiFrame(p);
        if (t == CHI_FRAME_CATCH)
            return p - 1;
        Chili clos;
        if (t == CHI_FRAME_UPDATE &&
            !chiThunkForced(p[-1], &clos) &&
            chiToCont(chiIdx(clos, 0)) == &chiThunkBlackhole)
            chiInit(clos, chiPayload(clos), p[-2]); // Reset thunk entry to non-blackholed
    }
    return 0;
}

#if CHI_POISON_ENABLED
enum { MIGRATE_MAX_DEPTH = 64 };

typedef struct {
    uint8_t oldOwner;
    uint32_t depth;
    Chili chain[MIGRATE_MAX_DEPTH];
    ChiBlockVec stack;
} MigrateState;

static void migrate(MigrateState*, Chili);

static void migrateLoop(MigrateState* state, Chili* p, Chili* end, Chili src) {
    state->chain[state->depth++] = src;
    for (; p < end; ++p)
        migrate(state, *p);
    --state->depth;
}

static void migrateRecursive(MigrateState* state, Chili c) {
    Chili* p = chiPayload(c);
    migrateLoop(state, p, p + chiSize(c), c);
}

static void migrate(MigrateState* state, Chili c) {
    if (chiUnboxed(c))
        return;

    ChiType type = chiType(c);
    if (chiRaw(type) || type == CHI_STACK)
        return;

    if (chiGen(c) != CHI_GEN_MAJOR) {
        char buf[256];
        ChiSinkMem s;
        chiSinkMemInit(&s, buf, sizeof (buf));
        for (uint32_t i = 0; i < state->depth; ++i)
            chiSinkFmt(&s.base, "%s%C", i ? " -> " : "", state->chain[i]);
        chiErr("Processor %u: Found minor object %C in migrated transitive closure, referenced by %b",
               _chiDebugData.wid, c, (uint32_t)s.used, s.buf);
    }

    ChiObject* obj = chiObjectUnchecked(c);
    if (chiObjectShared(obj) || chiObjectOwner(obj) == chiExpectedObjectOwner())
        return;

    if (chiObjectOwner(obj) != state->oldOwner)
        chiErr("Processor %u: Migrated object %C not owned by %u",
               _chiDebugData.wid, c, state->oldOwner - 1);

    chiObjectSetOwner(obj, chiExpectedObjectOwner());

    if (state->depth < MIGRATE_MAX_DEPTH)
        migrateRecursive(state, c);
    else
        chiBlockVecPush(&state->stack, c);
}

static void migrateStack(ChiProcessor* proc, Chili stack) {
    ChiObject* obj = chiObjectUnchecked(stack);
    uint8_t oldOwner = chiObjectOwner(obj);
    if (oldOwner == chiExpectedObjectOwner())
        return;
    chiObjectSetOwner(obj, chiExpectedObjectOwner());
    MigrateState state = { .oldOwner = oldOwner };
    chiBlockVecInit(&state.stack, &proc->heap.manager);
    ChiStack* s = chiToStack(stack);
    migrateLoop(&state, s->base, s->sp, stack);
    Chili c;
    while (chiBlockVecPop(&state.stack, &c))
        migrateRecursive(&state, c);
}
#else
static void migrateStack(ChiProcessor* CHI_UNUSED(proc), Chili CHI_UNUSED(stack)) {}
#endif

static bool scanActivatedStack(ChiLocalGC* gc, ChiObject* obj) {
    if (atomic_load_explicit(&gc->phase, memory_order_relaxed) != CHI_GC_ASYNC ||
        chiColorEq(chiObjectColor(obj), gc->markState.black))
        return false;
    chiObjectSetColor(obj, gc->markState.black);
    ChiStack* stack = (ChiStack*)obj->payload;
    for (Chili* p = stack->base; p < stack->sp; ++p)
        chiGrayMarkUnboxed(gc, *p);
    return true;
}

void chiStackActivate(ChiProcessor* proc) {
    Chili stack = chiThreadStackUnchecked(proc->thread);
    ChiObject* obj = chiObjectUnchecked(stack);
    chiObjectLock(obj);
    CHI_ASSERT(!chiObjectShared(obj));
    migrateStack(proc, stack);
    bool scanned = scanActivatedStack(&proc->gc, obj);
    chiEvent(proc, STACK_ACTIVATE, .stack = chiAddress(stack), .scanned = scanned);
}

void chiStackDeactivate(ChiProcessor* proc, Chili* sp) {
    Chili stack = chiThreadStackUnchecked(proc->thread);
    ChiObject* obj = chiObject(stack);
    ChiStack* s = chiToStack(stack);

    CHI_ASSERT(!chiObjectShared(obj));
    CHI_ASSERT(!sp || sp <= chiStackLimit(stack));
    CHI_ASSERT(!sp || sp >= s->base);

    s->sp = sp ? sp : s->base;

    // Mark dirty such that stack is scanned in scavenger,
    // since it can reference minor objects.
    if (sp && !chiObjectDirty(obj)) {
        chiObjectSetDirty(obj, true);
        chiBlockVecPush(&proc->heap.majorDirty, stack);
    }

    chiEvent(proc, STACK_DEACTIVATE, .stack = chiAddress(stack));
    chiObjectUnlock(obj);
}
