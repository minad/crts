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
        ChiObject* obj = chiObject(c);
        ChiWord* sl = obj->payload + obj->size;
        for (size_t i = 0; i < CHI_MAX(1U, CHI_POISON_STACK_CANARY_SIZE); ++i)
            *--sl = CHI_POISON_STACK_CANARY;
    }
}

static Chili stackTraceElement(ChiProcessor* proc, const ChiLocInfo* loc) {
    return chiNewTuple(proc,
                       chiStringNew(proc, loc->file),
                       chiStringNew(proc, loc->fn),
                       chiFromUnboxed(loc->line));
}

Chili chiStackNew(ChiProcessor* proc, size_t size) {
    // Stacks are always pinned for better cpu caching, since they are heavily mutated.
    // Furthermore we don't have to modify the stack pointer during gc when the stack would be moved.
    CHI_ASSERT(size > CHI_STACK_OVERHEAD);
    Chili c = chiNewInl(proc, CHI_STACK, size, CHI_NEW_LOCAL | CHI_NEW_CLEAN);
    stackInitCanary(c);
    ChiStack* stack = chiToStack(c);
    stack->sp = stack->base;
    return c;
}

uint32_t chiStackTryGrow(ChiProcessor* proc, Chili* sp, Chili* newLimit) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    Chili oldStackObj = chiThreadStack(proc->thread);
    ChiStack* oldStack = chiToStack(oldStackObj);

    // 2 for stackGrowCont frame
    size_t need = (size_t)(newLimit - sp) + 2 + CHI_STACK_OVERHEAD;

    // - copy at least one stack frame in order to keep continuations intact
    // - copy frame below chiRestoreCont, in order to keep continuation after restore intact
    size_t copied = 0;
    while (sp - copied > oldStack->base) {
        bool restore = chiToCont(*(sp - copied - 1)) == &chiRestoreCont;
        copied += chiFrameSize(sp - copied - 1);
        if (!restore && copied >= opt->stack.hot / CHI_WORDSIZE)
            break;
    }

    sp -= copied;
    CHI_ASSERT(sp >= oldStack->base);

    size_t newSize = CHI_MAX((size_t)(copied + need),
                             CHI_MIN(opt->stack.step / CHI_WORDSIZE, 2 * chiObject(oldStackObj)->size));

    if (chiToThread(proc->thread)->stackSize + newSize > opt->stack.limit / CHI_WORDSIZE &&
        chiThreadInterruptible(proc->thread)) {
        return CHI_STACK_OVERFLOW;
    }

    Chili newStackObj = chiStackNew(proc, newSize);
    if (!chiSuccess(newStackObj))
        return CHI_HEAP_OVERFLOW;

    ChiStack* newStack = chiToStack(newStackObj);
    if (sp > oldStack->base) {
        *newStack->sp++ = oldStackObj;
        *newStack->sp++ = chiFromCont(&chiStackGrowCont);
    }

    memcpy(newStack->sp, sp, CHI_WORDSIZE * copied);
    newStack->sp += copied;

    size_t totalSize = chiToThread(proc->thread)->stackSize += newSize;
    chiEvent(proc, STACK_GROW,
             .stack = chiAddress(newStackObj),
             .size = CHI_WORDSIZE * totalSize,
             .step = CHI_WORDSIZE * newSize,
             .copied = CHI_WORDSIZE * copied);

    chiStackDeactivate(proc, sp);
    chiAtomicWrite(&chiToThread(proc->thread)->stack, newStackObj);
    chiStackActivate(proc);
    chiStackDebugWalk(newStackObj, newStack->sp, chiStackLimit(chiThreadStack(proc->thread)));

    return UINT32_MAX;
}

void chiStackDump(ChiSink* sink, ChiProcessor* proc, Chili* sp) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    ChiStackWalk w = chiStackWalkInit(chiThreadStack(proc->thread), sp, opt->stack.trace, opt->stack.traceCycles);
    while (chiStackWalk(&w)) {
        ChiLocResolve resolve;
        chiLocResolve(&resolve, chiLocateFrame(w.frame), opt->stack.traceMangled);
        chiSinkFmt(sink, "%L;", &resolve.loc);
    }
    if (w.incomplete)
        chiSinkPuts(sink, "-;");

    Chili name = chiThreadName(proc->thread);
    uint32_t tid = chiThreadId(proc->thread);
    if (chiTrue(name))
        chiSinkFmt(sink, "[thread %u %S];[%s]", tid, chiStringRef(&name), proc->worker->name);
    else
        chiSinkFmt(sink, "[unnamed thread %u];[%s]", tid, proc->worker->name);
}

static uint32_t stackDepth(Chili stack, Chili* sp, uint32_t max, bool cycles) {
    ChiStackWalk w = chiStackWalkInit(stack, sp, max, cycles);
    while (chiStackWalk(&w));
    return w.depth;
}

Chili chiStackGetTrace(ChiProcessor* proc, Chili* sp) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    if (!opt->stack.trace)
        return CHI_FALSE;

    Chili stack = chiThreadStack(proc->thread);
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

void chiStackShrink(ChiProcessor* proc) {
    Chili oldStackObj = chiThreadStack(proc->thread);
    ChiStack* oldStack = chiToStack(oldStackObj);
    Chili newStackObj = *oldStack->base;
    CHI_ASSERT(chiType(newStackObj) == CHI_STACK);

    size_t step = chiObject(oldStackObj)->size,
        totalSize = chiToThread(proc->thread)->stackSize -= step;

    chiEvent(proc, STACK_SHRINK,
             .stack = chiAddress(newStackObj),
             .size = CHI_WORDSIZE * totalSize,
             .step = CHI_WORDSIZE * step);

    chiStackDeactivate(proc, 0);
    chiAtomicWrite(&chiToThread(proc->thread)->stack, newStackObj);
    chiStackActivate(proc);
    chiStackDebugWalk(newStackObj, chiToStack(newStackObj)->sp, chiStackLimit(newStackObj));
}

bool chiStackUnwind(ChiProcessor* proc, Chili* sp) {
    ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    stack->sp = sp;
 next:
    for (Chili* p = stack->sp - 1; p >= stack->base; p -= chiFrameSize(p)) {
        if (chiToCont(*p) == &chiExceptionCatchCont) {
            stack->sp = p - 1;
            return true;
        }
        if (chiToCont(*p) == &chiStackGrowCont) {
            CHI_ASSERT(p == stack->base + 1);
            chiStackShrink(proc);
            stack = chiToStack(chiThreadStack(proc->thread));
            goto next;
        }
        /* TODO restore blackholes for async exceptions
           Chili clos;
           if (t == CHI_FRAME_UPDATE &&
           !chiThunkForced(p[-1], &clos) &&
           chiToCont(p[-2]) != &chiThunkBlackhole)
           chiInit(clos, chiPayload(clos), p[-2]); // Reset thunk entry to non-blackholed
        */
    }
    stack->sp = stack->base;
    return false;
}

#if CHI_POISON_ENABLED
enum { MIGRATE_MAX_DEPTH = 64 };

typedef struct {
    uint8_t oldOwner;
    uint32_t depth;
    Chili chain[MIGRATE_MAX_DEPTH];
    ChiObjVec postponed;
} MigrateState;

static void migrateObject(MigrateState*, Chili);

static void migrateObject(MigrateState* state, Chili c) {
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

    if (state->depth < MIGRATE_MAX_DEPTH) {
        Chili *p, *end;
        if (chiType(c) == CHI_STACK) {
            ChiStack* s = chiToStack(c);
            p = s->base;
            end = s->sp;
        } else {
            p = chiPayload(c);
            end = p + chiSize(c);
        }
        state->chain[state->depth++] = c;
        for (; p < end; ++p) {
            Chili d = *p;
            if (chiRef(d)) {
                ChiType type = chiType(d);
                if (!chiRaw(type) && (type != CHI_STACK || chiType(c) == CHI_STACK))
                    migrateObject(state, d);
            }
        }
        --state->depth;
    } else {
        chiObjVecPush(&state->postponed, c);
    }
}

static void migrate(ChiProcessor* proc, Chili c) {
    ChiObject* obj = chiObjectUnchecked(c);
    uint8_t oldOwner = chiObjectOwner(obj);
    if (oldOwner == chiExpectedObjectOwner())
        return;
    MigrateState state = { .oldOwner = oldOwner };
    chiObjVecInit(&state.postponed, &proc->heap.manager);
    migrateObject(&state, c);
    while (chiObjVecPop(&state.postponed, &c))
        migrateObject(&state, c);
    chiObjVecFree(&state.postponed);
}
#else
static void migrate(ChiProcessor* CHI_UNUSED(proc), Chili CHI_UNUSED(stack)) {}
#endif

static void scanActivatedStack(ChiProcessor* proc, ChiLocalGC* gc, Chili stack) {
    ChiObject* obj = chiObject(stack);
    if (atomic_load_explicit(&gc->phase, memory_order_relaxed) != CHI_GC_ASYNC ||
        chiColorEq(chiObjectColor(obj), gc->markState.black))
        return;
    chiObjectSetColor(obj, gc->markState.black);
    ChiStack* s = (ChiStack*)obj->payload;
    for (Chili* p = s->base; p < s->sp; ++p)
        chiMark(gc, *p);
    chiEvent(proc, STACK_SCANNED, .stack = chiAddress(stack));
}

void chiStackActivate(ChiProcessor* proc) {
    Chili stack = chiThreadStackUnchecked(proc->thread);
    ChiObject* obj = chiObjectUnchecked(stack);
    chiObjectLock(obj);
    CHI_ASSERT(!chiObjectShared(obj));
    migrate(proc, stack);
    chiEvent(proc, STACK_ACTIVATE, .stack = chiAddress(stack));
    scanActivatedStack(proc, &proc->gc, stack);
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
    if (sp && !obj->flags.dirty) {
        obj->flags.dirty = true;
        chiObjVecPush(&proc->heap.dirty.objects, stack);
    }

    chiEvent(proc, STACK_DEACTIVATE, .stack = chiAddress(stack));
    chiObjectUnlock(obj);
}
