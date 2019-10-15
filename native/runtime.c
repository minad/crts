#include <chili/cont.h>
#include <stdlib.h>
#include "barrier.h"
#include "error.h"
#include "event.h"
#include "exception.h"
#include "export.h"
#include "new.h"
#include "sink.h"
#include "stack.h"
#include "tracepoint.h"

#define ASSERT_INVARIANTS                                               \
    ({                                                                  \
        CHI_ASSERT(chiIdentical(AUX.THREAD, CHI_CURRENT_PROCESSOR->thread)); \
        CHI_ASSERT(chiIdentical(AUX.PROC, chiFromPtr(CHI_CURRENT_PROCESSOR))); \
        CHI_ASSERT(chiIdentical(AUX.PLOCAL, CHI_CURRENT_PROCESSOR->local)); \
        CHI_ASSERT(SP >= getStack(AUX.THREAD)->base);                         \
        CHI_ASSERT(SP <= getStackLimit(AUX.THREAD));                        \
        CHI_ASSERT(SL == getStackLimit(AUX.THREAD));                        \
        CHI_ASSERT(HP >= chiLocalHeapBumpBase(&CHI_CURRENT_PROCESSOR->heap)); \
        CHI_ASSERT(HP <= chiLocalHeapBumpLimit(&CHI_CURRENT_PROCESSOR->heap)); \
        CHI_ASSERT(NA < CHI_AMAX);                                      \
        chiStackDebugWalk(AUX.THREAD, SP, SL);                          \
    })

#define PROLOGUE_INVARIANTS(c) PROLOGUE(c); ASSERT_INVARIANTS

#define RESTORE_STACK_REGS                      \
    ({                                          \
        CHI_ASSERT(chiIdentical(proc->thread, AUX.THREAD)); \
        SP = getStack(AUX.THREAD)->sp;               \
        SLRW = getStackLimit(AUX.THREAD);            \
        CHI_ASSERT(SP >= getStack(AUX.THREAD)->base);  \
        CHI_ASSERT(SP <= SL);                    \
    })

#define ACTIVATE_THREAD                         \
    ({                                          \
        chiStackActivate(proc);                 \
        AUX.THREAD = proc->thread;              \
        RESTORE_STACK_REGS;                     \
        ASSERT_INVARIANTS;                      \
        chiEvent0(proc, THREAD_RUN_BEGIN);      \
    })

#define DEACTIVATE_THREAD                       \
    ({                                          \
        chiEvent0(proc, THREAD_RUN_END);        \
        chiStackDeactivate(proc, SP);           \
    })

#define RESTORE_HEAP_REGS                                               \
    ({                                                                  \
        ChiWord* base = chiLocalHeapBumpBase(&proc->heap);               \
        ChiBlockManager* bm = &proc->heap.manager;                 \
        /* HP - 1 points inside of block, check if block changed */     \
        if (!HP || chiBlock(bm, base) != chiBlock(bm, HP - 1)) {        \
            HP = base;                                                  \
            HLRW = chiLocalHeapBumpLimit(&proc->heap);                   \
            if (chiTriggered(&proc->trigger.interrupt))                 \
                HLRW = 0; /* restore interrupt trigger */               \
        }                                                               \
    })

CALLCONV_REGSTORE

CHI_INL ChiStack* getStack(Chili thread) {
    return chiToStack(chiThreadStack(thread));
}

CHI_INL Chili* getStackLimit(Chili thread) {
    return chiStackLimit(chiThreadStack(thread));
}

STATIC_CONT(thunkUpdateCont, .type = CHI_FRAME_UPDATE, .size = 3) {
    PROLOGUE_INVARIANTS(thunkUpdateCont);
    SP -= 3;
    Chili thunk = SP[1], val = A(0);
    chiFieldWrite(CHI_CURRENT_PROCESSOR, thunk, &chiToThunk(thunk)->val, val);
    RET(val);
}

STATIC_CONT(catchCont, .type = CHI_FRAME_CATCH, .size = 2) {
    PROLOGUE_INVARIANTS(catchCont);
    SP -= 2;
    RET(A(0));
}

#ifdef CHI_STANDALONE_WASM
// TODO: Wasm linker does not yet support --defsym
// see also main/wasm.c
extern int32_t wasmMainIdx(void);
#  define MAIN_IDX wasmMainIdx()
#else
#  define MAIN_IDX z_Main_z_main
#endif

INTERN_CONT(chiRunMainCont, .size = 1) {
    PROLOGUE_INVARIANTS(chiRunMainCont);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (MAIN_IDX < -1)
        CHI_THROW_RUNTIME_EX(proc, chiStringNew(proc, "No main function"));

    --SP;

    Chili mainMod = A(0);
    A(0) = proc->rt->entryPoint.start;
    A(1) = MAIN_IDX >= 0 ? chiIdx(mainMod, (size_t)MAIN_IDX) : mainMod;
    A(2) = CHI_FALSE;
    APP(2);
}

INTERN_CONT(chiRestoreCont, .type = CHI_FRAME_RESTORE, .size = CHI_VASIZE) {
    PROLOGUE_INVARIANTS(chiRestoreCont);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;

    if (CHI_SYSTEM_HAS_INTERRUPT
        && chiTriggered(&proc->trigger.userInterrupt)
        && chiTrue(rt->entryPoint.userInterrupt)
        && chiThreadInterruptible(proc->thread)) {
        chiTrigger(&proc->trigger.userInterrupt, false);
        A(0) = rt->entryPoint.userInterrupt;
        A(1) = CHI_FALSE;
        APP(1);
    }

    if (chiTriggered(&proc->trigger.tick)
        && chiThreadInterruptible(proc->thread)
        && chiTrue(rt->entryPoint.timerInterrupt)) {
        chiTrigger(&proc->trigger.tick, false);
        A(0) = rt->entryPoint.timerInterrupt;
        A(1) = CHI_FALSE;
        APP(1);
    }

    size_t size = (size_t)chiToUnboxed(SP[-2]);
    SP -= size;

    NARW = (uint8_t)(size - 4);
    for (size_t i = 0; i <= NA; ++i)
        A(i) = SP[i + 1];

    JUMP_FN(SP[0]);
}

INTERN_CONT(chiJumpCont, .size = 2) {
    PROLOGUE_INVARIANTS(chiJumpCont);
    SP -= 2;
    JUMP_FN(SP[0]);
}

INTERN_CONT(chiSetupEntryPointsCont, .size = 1) {
    PROLOGUE_INVARIANTS(chiSetupEntryPointsCont);
    --SP;

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;
    const Chili mod = A(0);
    if (!chiSuccess(mod))
        RET(CHI_FALSE);

    ChiEntryPoint* ep = &rt->entryPoint;

    chiGCUnroot(rt, ep->unhandled); // Remove root registered in setup()

    ep->start = chiIdx(mod, (size_t)z_System__EntryPoints_z_start);
    ep->unhandled = chiIdx(mod, (size_t)z_System__EntryPoints_z_unhandled);
    ep->blackhole = chiIdx(mod, (size_t)z_System__EntryPoints_z_blackhole);
    ep->userInterrupt = chiIdx(mod, (size_t)z_System__EntryPoints_z_userInterrupt);
    ep->timerInterrupt = chiIdx(mod, (size_t)z_System__EntryPoints_z_timerInterrupt);

    CHI_ASSERT(chiFnArity(ep->start) <= 2);
    CHI_ASSERT(chiFnArity(ep->unhandled) <= 2);
    CHI_ASSERT(chiFnArity(ep->userInterrupt) <= 1);
    CHI_ASSERT(chiFnArity(ep->timerInterrupt) <= 1);
    CHI_ASSERT(chiFnArity(ep->blackhole) <= 1);

    ChiPromoteState ps;
    chiPromoteSharedBegin(&ps, proc);
    chiPromoteSharedObject(&ps, &ep->start);
    chiPromoteSharedObject(&ps, &ep->unhandled);
    chiPromoteSharedObject(&ps, &ep->blackhole);
    chiPromoteSharedObject(&ps, &ep->userInterrupt);
    chiPromoteSharedObject(&ps, &ep->timerInterrupt);
    chiPromoteSharedEnd(&ps);

    chiGCRoot(rt, ep->start);
    chiGCRoot(rt, ep->unhandled);
    chiGCRoot(rt, ep->blackhole);
    chiGCRoot(rt, ep->userInterrupt);
    chiGCRoot(rt, ep->timerInterrupt);

    RET(CHI_FALSE);
}

INTERN_CONT(chiStartupEndCont, .size = 1) {
    PROLOGUE_INVARIANTS(chiStartupEndCont);
    --SP;
    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    chiEvent0(rt, STARTUP_END);
    RET(CHI_FALSE);
}

CONT(chiExceptionThrow) {
    PROLOGUE_INVARIANTS(chiExceptionThrow);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;

    const Chili except = A(0);
    const Chili info = chiIdx(except, 0);
    const Chili identifier = chiIdx(info, 0);
    const Chili name = chiIdx(identifier, 1);

    Chili* catchFrame = chiStackUnwind(proc, SP);

    if (chiEventEnabled(proc, EXCEPTION)) {
        CHI_STRING_SINK(sink);
        chiStackDump(sink, proc, SP);
        chiEvent(proc, EXCEPTION, .handled = !!catchFrame, .trace = chiSinkString(sink), .name = chiStringRef(&name));
    }

    if (catchFrame) {
        SP = catchFrame;
        A(0) = *SP;
        A(1) = except;
        APP(1);
    }

    chiThreadSetInterruptible(proc, false);
    SP = getStack(proc->thread)->base;
    A(0) = proc->rt->entryPoint.unhandled;
    A(1) = except;
    A(2) = CHI_FALSE;
    APP(2);
}

CONT(chiThreadSwitch) {
    PROLOGUE_INVARIANTS(chiThreadSwitch);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    Chili next = A(0);

    chiEvent(proc, THREAD_SWITCH, .nextTid = chiThreadId(next));

    DEACTIVATE_THREAD;
    proc->thread = next;
    ACTIVATE_THREAD;

    RET(CHI_FALSE);
}

CONT(chiProtectEnd) {
    _PROLOGUE(0); // invalid stack frame, use _PROLOGUE which does not have a tracepoint
    CHI_ASSERT(chiUnboxed(A(0)) || chiGen(A(0)) == CHI_GEN_MAJOR);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiProcessorProtectEnd(proc);
    RESTORE_HEAP_REGS;
    ACTIVATE_THREAD;

    const ChiCont _chiCurrentCont = &chiProtectEnd;
    CHI_NOWARN_UNUSED(_chiCurrentCont);
    TRACE_FFI(protect);

    RET(A(0));
}

#if !CHI_SYSTEM_HAS_INTERRUPT
static void pollTick(ChiProcessor* proc) {
    ChiNanos time = chiClock(CHI_CLOCK_REAL_FAST);
    if (chiNanosLess(proc->trigger.lastTick, time)) {
        chiTrigger(&proc->trigger.interrupt, true);
        chiTrigger(&proc->trigger.tick, true);
        proc->trigger.lastTick = chiNanosAdd(time, chiMillisToNanos(proc->rt->option.interval));
    }
}
#endif

/**
 * chiInterrupt is the central entry point to the runtime.
 * In particular it acts as a safe point for garbage collection.
 *
 * * Stack resizing
 * * Calling the timer interrupt handler
 * * Supplying fresh blocks for bump allocation, see ::chiLocalHeapBumpGrow.
 * * Handling stack dump signals, see ::chiStackDump.
 * * Handling processor service requests via ::chiProcessorService
 */
CONT(chiInterrupt, .na = CHI_VAARGS) {
    PROLOGUE(chiInterrupt); // invariant violated since SL is modified

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;

    CHI_UNLESS(CHI_SYSTEM_HAS_INTERRUPT, pollTick(proc);)

    /* Limits are used to pass arguments.
     * Thus we have to restore the old limits.
     */
    ChiWord* newHL = HL;
    Chili* newSL = SL;
    HLRW = chiLocalHeapBumpLimit(&proc->heap);
    SLRW = getStackLimit(proc->thread);
    ASSERT_INVARIANTS;

    if (!CHI_TRACEPOINTS_CONT_ENABLED)
        CHI_TTP(proc, .name = "unknown native", .thread = true);

    /* Grow heap if requested.
     * If an interrupt occurs just in the right momemt,
     * it sets HL=newHL=0. Then we fail to fulfill the request.
     * However the heap limit check will be done again,
     * when the continuation is restarted.
     */
    if (newHL > chiLocalHeapBumpLimit(&proc->heap)) {
        chiLocalHeapBumpGrow(&proc->heap);
        RESTORE_HEAP_REGS;
        ASSERT_INVARIANTS;
        if (chiThreadInterruptible(proc->thread)
            && chiTriggerExchange(&rt->heapOverflow, false))
            KNOWN_JUMP(chiHeapOverflow);
    }

    // Ensure that there is enough stack space for restore frame
    // restore frame: cont, a0, ..., a[na], size, chiRestoreCont
    uint32_t na = chiFrameArgs(SP, A(0), NA);
    newSL = CHI_MAX(newSL, SP + na + 4);

    if (newSL != getStackLimit(proc->thread) && // resize requested
        newSL > getStack(proc->thread)->base + rt->option.stack.max && // request to large
        chiThreadInterruptible(proc->thread))
        CHI_THROW_ASYNC_EX(proc, CHI_STACK_OVERFLOW);

    Chili newStack = chiStackTryResize(proc, SP, newSL);
    if (!chiSuccess(newStack))
        KNOWN_JUMP(chiHeapOverflow);

    if (!chiIdentical(newStack, chiThreadStack(proc->thread))) {
        chiStackDeactivate(proc, 0);
        chiAtomicWrite(&chiToThread(proc->thread)->stack, newStack);
        chiStackActivate(proc);
        CHI_TTP(proc, .cont = chiToCont(SP[0]), .name = "stack resize", .thread = true);
    }

    // Restore stack registers, stack might have been resized (Even if it is the same object!)
    RESTORE_STACK_REGS;
    ASSERT_INVARIANTS;

    if (!chiTriggered(&proc->trigger.interrupt))
        JUMP_FN(SP[0]);

    // Create restore frame, save thread
    CHI_ASSUME(na <= CHI_AMAX);
    for (uint32_t i = 0; i <= na; ++i)
        SP[i + 1] = A(i);
    SP[na + 2] = chiFromUnboxed((ChiWord)na + 4);
    SP[na + 3] = chiFromCont(&chiRestoreCont);
    SP += na + 4;
    ASSERT_INVARIANTS;

    DEACTIVATE_THREAD;

    if (chiTriggered(&proc->trigger.interrupt)) {
        chiEvent0(proc, PROC_RUN_END);
        chiProcessorService(proc);
        chiEvent0(proc, PROC_RUN_BEGIN);
    }

    RESTORE_HEAP_REGS;
    ACTIVATE_THREAD;

    CHI_ASSERT(chiToCont(SP[-1]) == &chiRestoreCont);
    KNOWN_JUMP(chiRestoreCont);
}

CONT(chiThunkForce, .na = 1) {
    PROLOGUE_INVARIANTS(chiThunkForce);
    LIMITS(.stack=3);

    Chili clos = A(0), thk = A(1), fn = chiIdx(clos, 0);

    /* Racing: Overwrite the closure entry point
     * with the blackhole. This even works on 32 bit architectures,
     * since then continuation pointer lives in the lower 32 bits.
     * Therefore no reads of invalid addresses are possible.
     * However on 32 bit architectures without prefix data,
     * the upper 32 bits of the value are used to store the pointer
     * to the continuation function. Therefore it could be possible to read
     * an inconsistent continuation information/continuation function pointer.
     * Fortunately this is not a problem, since the relevant continuation
     * information (na and size) are equal for all thunk functions.
     */
    chiInit(clos, chiPayload(clos), chiFromCont(&chiThunkBlackhole));

    SP[0] = fn;
    SP[1] = thk;
    SP[2] = chiFromCont(&thunkUpdateCont);
    SP += 3;

    JUMP_FN(fn);
}

INTERN_CONT(chiThunkForward) {
    PROLOGUE_INVARIANTS(chiThunkForward);
    FORCE(chiIdx(A(0), 1));
}

STATIC_CONT(blackholeCont) {
    PROLOGUE_INVARIANTS(blackholeCont);
    --SP;
    KNOWN_JUMP(chiThunkBlackhole);
}

INTERN_CONT(chiThunkBlackhole) {
    PROLOGUE_INVARIANTS(chiThunkBlackhole);
    LIMITS(.stack = 1);
    Chili thunk = SP[-2], clos;
    if (!chiThunkForced(thunk, &clos)) {
        // Retry thunk if it has been restored to a non-blackhole
        Chili fn = chiIdx(clos, 0);
        if (chiToCont(fn) != &chiThunkBlackhole)
            JUMP_FN(fn);
        *SP++ = chiFromCont(&blackholeCont);
        A(0) = CHI_CURRENT_RUNTIME->entryPoint.blackhole;
        A(1) = CHI_FALSE;
        APP(1);
    }
    SP -= 3;
    RET(clos);
}

INTERN_CONT(chiShowAsyncException, .na = 1) {
    PROLOGUE_INVARIANTS(chiShowAsyncException);
    static const char* const asyncExceptionName[] =
        { "HeapOverflow",
          "StackOverflow",
          "ThreadInterrupt",
          "UserInterrupt" };
    RET(chiStringFromRef(chiStringRef(asyncExceptionName[chiToUInt32(A(1))])));
}

CONT(chiStackTrace) {
    PROLOGUE_INVARIANTS(chiStackTrace);
    RET(chiStackGetTrace(CHI_CURRENT_PROCESSOR, SP));
}

INTERN_CONT(chiIdentity, .na = 1) {
    PROLOGUE_INVARIANTS(chiIdentity);
    RET(A(1));
}

STATIC_CONT(showUnhandledCont, .size = 2) {
    PROLOGUE_INVARIANTS(showUnhandledCont);
    SP -= 2;
    Chili name = SP[0], except = A(0);
    chiSinkFmt(chiStderr, "Unhandled exception occurred:\n%S: %S\n",
               chiStringRef(&name), chiStringRef(&except));
    chiExit(1);
    RET(CHI_FALSE);
}

INTERN_CONT(chiEntryPointUnhandledDefault, .na = 2) {
    PROLOGUE_INVARIANTS(chiEntryPointUnhandledDefault);
    LIMITS(.stack = 2);

    const Chili except = A(1);
    const Chili info = chiIdx(except, 0);
    const Chili identifier = chiIdx(info, 0);
    const Chili name = chiIdx(identifier, 1);

    SP[0] = name;
    SP[1] = chiFromCont(&showUnhandledCont);
    SP += 2;

    A(0) = chiIdx(info, 1);
    A(1) = chiIdx(except, 1);
    APP(1);
}

CONT(chiHeapOverflow) {
    PROLOGUE_INVARIANTS(chiHeapOverflow);
    CHI_THROW_ASYNC_EX(CHI_CURRENT_PROCESSOR, CHI_HEAP_OVERFLOW);
}

CONT(chiExceptionCatch, .na = 1) {
    PROLOGUE_INVARIANTS(chiExceptionCatch);
    LIMITS(.stack=3);

    SP[0] = A(1);
    SP[1] = chiFromCont(&catchCont);
    SP += 2;

    A(1) = CHI_FALSE;
    APP(1);
}

CONT(chiMigrate) {
    PROLOGUE_INVARIANTS(chiMigrate);
    chiProcessorRequest(CHI_CURRENT_PROCESSOR, CHI_REQUEST_MIGRATE);
    RET(CHI_FALSE);
}

STATIC_CONT(returnUnit) {
    PROLOGUE_INVARIANTS(returnUnit);
    RET(CHI_FALSE);
}

static void parkEnd(ChiProcessor* proc) {
    CHI_TTP(proc, .name = "runtime;park");
    chiEvent0(proc, PROC_PARK_END);
    chiEvent0(proc, PROC_RUN_BEGIN);
}

static void parkBegin(ChiProcessor* proc) {
    chiThreadSetInterruptible(proc, true);
    chiEvent0(proc, PROC_RUN_END);
    chiEvent0(proc, PROC_PARK_BEGIN);
}

#ifdef CHI_STANDALONE_WASM
// HACK: Works only with *_tls callconv, since the regstore must be global
#define CHECK_CALLCONV_trampoline_tls 1
#ifdef __wasm_tail_call__
#  define CHECK_CALLCONV_tail_tls 1
#endif
_Static_assert(CHI_CAT(CHECK_CALLCONV_,CHI_CALLCONV), "Invalid callconv for wasm");
_Noreturn void chiWasmContinue(void) {
    _PROLOGUE(0);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    parkEnd(proc);
    chiEvent0(proc, THREAD_RUN_BEGIN);
    FIRST_JUMP(returnUnit);
}

CONT(chiProcessorPark) {
    PROLOGUE_INVARIANTS(chiProcessorPark);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiEvent0(proc, THREAD_RUN_END);
    parkBegin(proc);
    wasm_throw(&(wasm_exception_t){ WASM_PARK, (uint32_t)CHI_UN(Millis, proc->suspend.timeout) });
}
#else
static void park(ChiProcessor* proc) {
    parkBegin(proc);
    {
        CHI_LOCK_MUTEX(&proc->suspend.mutex);
        while (!chiMillisZero(proc->suspend.timeout)) {
            ChiMillis waited = chiNanosToMillis(chiCondTimedWait(&proc->suspend.cond,
                                                                 &proc->suspend.mutex,
                                                                 chiMillisToNanos(proc->suspend.timeout)));
            proc->suspend.timeout = chiMillisDelta(proc->suspend.timeout, waited);
        }
    }
    parkEnd(proc);
}

CONT(chiProcessorPark) {
    PROLOGUE_INVARIANTS(chiProcessorPark);
    PROTECT_VOID(park(CHI_CURRENT_PROCESSOR));
}
#endif

CONT(chiProcessorInitLocal) {
    PROLOGUE_INVARIANTS(chiProcessorInitLocal);
    CHI_ASSERT(!chiTrue(AUX.PLOCAL));
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    Chili local = A(0);
    chiPromoteLocal(proc, &local);
    AUX.PLOCAL = proc->local = local;
    RET(CHI_FALSE);
}

_Noreturn void chiProcessorEnter(ChiProcessor* proc) {
    CALLCONV_INIT;
    proc->reg.aux = &AUX;
    proc->reg.hl = &HLRW;
    CHI_IF(CHI_TRACEPOINTS_CONT_ENABLED, proc->worker->tp = &AUX.TRACEPOINT);
    chiHookRun(CHI_HOOK_PROC_START, proc);
    AUX.PROC = chiFromPtr(proc);
    CHI_IF(CHI_FNLOG_ENABLED, AUX.FNLOG = chiEventEnabled(proc, FNLOG_CONT);)
    chiEvent0(proc, PROC_RUN_BEGIN);
    RESTORE_HEAP_REGS;
    ACTIVATE_THREAD;
    ASSERT_INVARIANTS;
    FIRST_JUMP(returnUnit);
}

CONT(chiOverAppN, .size = CHI_VASIZE) {
    PROLOGUE_INVARIANTS(chiOverAppN);

    const size_t rest = (size_t)chiToUnboxed(SP[-2]) - 2;
    SP -= rest + 2;

    CHI_ASSUME(rest <= CHI_AMAX);
    for (size_t i = 0; i < rest; ++i)
        A(i + 1) = SP[i];

    APP(rest);
}

CONT(chiPartialNofM, .na = CHI_VAARGS) {
    PROLOGUE_INVARIANTS(chiPartialNofM);

    Chili clos = A(0);
    const size_t arity = chiFnArity(clos), args = chiSize(clos) - 2;

    CHI_ASSUME(arity <= CHI_AMAX);
    for (size_t i = arity; i > 0; --i)
        A(args + i) = A(i);

    CHI_ASSUME(args <= CHI_AMAX - 1);
    for (size_t i = 0; i <= args; ++i)
        A(i) = chiIdx(clos, i + 1);

    JUMP_FN0;
}

CONT(chiAppN, .type = CHI_FRAME_APPN, .na = CHI_VAARGS) {
    PROLOGUE_INVARIANTS(chiAppN);

    const size_t arity = chiFnArity(A(0));
    CHI_ASSERT(arity < CHI_AMAX);

    if (NA < arity) {
        //LIMITS(.heap=(size_t)NA + 2);
        LIMITS(.heap=CHI_AMAX + 2);
        PARTIAL(NA, arity);
    }

    if (NA > arity) {
        //LIMITS(.stack=(size_t)NA - arity + 2);
        LIMITS(.stack=CHI_AMAX + 2);
        OVERAPP(NA, arity);
    }

    JUMP_FN0;
}

#define DEFINE_APP(a)                                   \
    CONT(chiApp##a, .na = a) {                         \
        PROLOGUE_INVARIANTS(chiApp##a);                 \
                                                        \
        const size_t arity = chiFnArity(A(0));          \
        CHI_ASSERT(arity < CHI_AMAX);                   \
                                                        \
        if (a < arity) {                                \
            /*LIMITS(.heap=a + 2);*/                    \
            LIMITS(.heap=CHI_AMAX + 2);                 \
            PARTIAL(a, arity);                          \
        }                                               \
                                                        \
        if (a > arity) {                                \
            /*LIMITS(.stack=a - arity + 2);*/           \
            LIMITS(.stack=CHI_AMAX + 2);                \
            OVERAPP(a, arity);                          \
        }                                               \
                                                        \
        JUMP_FN0;                                       \
    }

DEFINE_APP(1)
DEFINE_APP(2)
DEFINE_APP(3)
DEFINE_APP(4)

#define DEFINE_PARTIAL(args, arity)                             \
    CONT(chiPartial##args##of##arity, .na = arity - args) {     \
        PROLOGUE_INVARIANTS(chiPartial##args##of##arity);       \
        Chili clos = A(0);                                      \
        CHI_ASSERT(chiFnArity(clos) == arity - args);           \
        CHI_ASSERT(chiSize(clos) - 2 == args);                  \
                                                                \
        for (size_t i = arity - args; i > 0; --i)               \
            A(args + i) = A(i);                                 \
                                                                \
        for (size_t i = 0; i < args + 1; ++i)                   \
            A(i) = chiIdx(clos, i + 1);                         \
                                                                \
        JUMP_FN0;                                               \
    }

DEFINE_PARTIAL(1, 2)
DEFINE_PARTIAL(1, 3)
DEFINE_PARTIAL(1, 4)
DEFINE_PARTIAL(2, 3)
DEFINE_PARTIAL(2, 4)
DEFINE_PARTIAL(3, 4)

#define DEFINE_OVERAPP(rest)                    \
    CONT(chiOverApp##rest, .size = rest + 1) {  \
        PROLOGUE_INVARIANTS(chiOverApp##rest);  \
                                                \
        SP -= rest + 1;                         \
                                                \
        CHI_ASSUME(rest <= CHI_AMAX);           \
        for (size_t i = 0; i < rest; ++i)       \
            A(i + 1) = SP[i];                   \
                                                \
        APP(rest);                              \
    }

DEFINE_OVERAPP(1)
DEFINE_OVERAPP(2)
DEFINE_OVERAPP(3)
DEFINE_OVERAPP(4)
