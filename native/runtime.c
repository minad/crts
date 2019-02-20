#include <chili/cont.h>
#include <chili/object/thunk.h>
#include "event.h"
#include "trace.h"
#include "exception.h"
#include "stack.h"
#include "runtime.h"
#include "export.h"
#include "barrier.h"
#include "sink.h"
#include "error.h"
#include <stdlib.h>

#define ASSERT_INVARIANTS                                               \
    ({                                                                  \
        CHI_ASSERT(SP >= getStackBase(CHI_CURRENT_PROCESSOR));          \
        CHI_ASSERT(SP <= getStackLimit(CHI_CURRENT_PROCESSOR));         \
        CHI_ASSERT(SL == getStackLimit(CHI_CURRENT_PROCESSOR));         \
        CHI_ASSERT(HP >= chiNurseryPtr(&CHI_CURRENT_PROCESSOR->nursery)); \
        CHI_ASSERT(HP <= chiNurseryLimit(&CHI_CURRENT_PROCESSOR->nursery)); \
        CHI_ASSERT(NA < CHI_AMAX);                                      \
        chiStackCheckCanary(SL);                                       \
        chiStackDebugWalk(getStackBase(CHI_CURRENT_PROCESSOR), SP);    \
    })

#define ENTER \
    ({ CHI_ASSERT(chiToCont(SP[-1]) == &chiEnterCont); KNOWN_JUMP_NOTRACE(chiEnterCont); })

#define PROLOGUE_INVARIANTS(c)                       \
    PROLOGUE(c); ASSERT_INVARIANTS

#define PROLOGUE_LIMITS(c, ...)                              \
    PROLOGUE_INVARIANTS(c); LIMITS(__VA_ARGS__)

#define JUMP_TO_INTERRUPT(cont)                                 \
    ({                                                          \
        CHI_CURRENT_PROCESSOR->trigger.interrupt = true;        \
        SP[0] = chiFromCont(&(cont));                           \
        KNOWN_JUMP(chiInterrupt);                               \
    })

#define STACK_DUMP(proc, event, ...)                            \
    ({                                                          \
        if (CHI_EVENT_P((proc), event)) {                       \
            CHI_STRING_SINK(sink);                              \
            chiStackDump(sink, (proc));                         \
            CHI_EVENT((proc), event,                            \
                      .trace = chiSinkString(sink),             \
                      ##__VA_ARGS__);                           \
        }                                                       \
    })

#define RESTORE_STACK_REGS                      \
    ({                                          \
        SP = getStack(proc)->sp;                \
        SLRW = getStackLimit(proc);             \
    })

#define RESTORE_HEAP_REGS                                       \
    ({                                                          \
        HP = chiNurseryPtr(&proc->nursery);          \
        HLRW = chiNurseryLimit(&proc->nursery);      \
    })

CALLCONV_REGSTORE

CHI_INL ChiStack* getStack(ChiProcessor* proc) {
    return chiToStack(chiThreadStack(proc->thread));
}

CHI_INL Chili* getStackLimit(ChiProcessor* proc) {
    return chiStackLimit(chiThreadStack(proc->thread));
}

CHI_INL Chili* getStackBase(ChiProcessor* proc) {
    return getStack(proc)->base;
}

STATIC_CONT(returnUnit) {
    PROLOGUE_LIMITS(returnUnit);
    RET(CHI_FALSE);
}

STATIC_CONT(exitHandler) {
    _PROLOGUE;
    exit(CHI_CURRENT_RUNTIME->exitCode);
}

CONT(chiExitDefault) {
    PROLOGUE_INVARIANTS(chiExitDefault);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    int32_t code = chiToInt32(A(0));
    CHI_ASSERT(code >= 0);
    proc->rt->exitCode = code;
    if (!proc->rt->option.fastExit) {
        CHI_THREAD_EVENT(END, proc);
        chiStackDeactivateReset(proc);
        CHI_EVENT0(proc->rt, SHUTDOWN_BEGIN);
        chiProcessorSync(proc);
    }
    KNOWN_JUMP_NOTRACE(exitHandler);
}

CHI_CONT_ALIAS(chiExitDefault, chiExit)

CONT(chiProcessorStop) {
    PROLOGUE_INVARIANTS(chiProcessorStop);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_THREAD_EVENT(END, proc);
    chiStackDeactivateReset(proc);
    chiProcessorStopped(proc);
}

STATIC_CONT(updateCont,, .type = CHI_FRAME_UPDATE, .size = 2) {
    PROLOGUE_LIMITS(updateCont);
    SP -= 2;
    Chili thunk = SP[0], val = A(0), clos;
    if (!chiThunkForced(thunk, &clos))
        chiWriteField(CHI_CURRENT_PROCESSOR, thunk, &chiToThunk(thunk)->val, val);
    RET(val);
}

STATIC_CONT(catchCont,, .type = CHI_FRAME_CATCH, .size = 2) {
    PROLOGUE_LIMITS(catchCont);
    SP -= 2;
    RET(A(0));
}

CONT(chiEntryPointExitDefault,, .na = 1) {
    PROLOGUE_LIMITS(chiEntryPointExitDefault);
    A(0) = CHI_FALSE;
    KNOWN_JUMP(chiExit);
}

#ifdef CHI_STANDALONE_WASM
// TODO: Wasm linker does not yet support --defsym
// see also main/wasm.c
extern int32_t wasmMainIdx(void);
#  define MAIN_IDX wasmMainIdx()
#else
#  define MAIN_IDX z_Main_z_main
#endif

CONT(chiRunMainCont,, .size = 1) {
    PROLOGUE_LIMITS(chiRunMainCont);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (MAIN_IDX < -1)
        CHI_THROW_RUNTIME_EX(proc, STATIC_STRING("No main function"));

    --SP;

    Chili mainMod = A(0);
    A(0) = MAIN_IDX >= 0 ? IDX(mainMod, (size_t)MAIN_IDX) : mainMod;
    A(1) = proc->rt->entryPoint.exit;
    APP(1);
}

CONT(chiEnterCont,, .type = CHI_FRAME_ENTER, .size = CHI_VASIZE) {
    PROLOGUE_INVARIANTS(chiEnterCont);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;

    if (CHI_UNLIKELY(proc->trigger.dumpStack)) {
        proc->trigger.dumpStack = false;
        STACK_DUMP(proc, STACK_TRACE);
    }

    bool heapOverflow = rt->heapOverflow;
    if (heapOverflow && !chiThreadExceptionBlock(proc->thread) &&
        atomic_compare_exchange_strong(&rt->heapOverflow, &heapOverflow, false))
        KNOWN_JUMP(chiHeapOverflow);

    if (CHI_SYSTEM_HAS_INTERRUPT) {
        uint32_t userInterrupt = rt->interruptSupport->userInterrupt;
        if (rt->interruptSupport->userInterrupt &&
            atomic_compare_exchange_strong(&rt->interruptSupport->userInterrupt, &userInterrupt, 0)) {
            A(0) = rt->entryPoint.interrupt;
            A(1) = rt->enterContClos;
            APP(1);
        }
    }

    size_t size = (size_t)chiToUnboxed(SP[-2]);
    SP -= size;

    NARW = (uint8_t)(size - 4);
    for (size_t i = 0; i <= NA; ++i)
        A(i) = SP[i + 1];

    JUMP_FN(SP[0]);
}

CONT(chiJumpCont,, .size = 2) {
    PROLOGUE_LIMITS(chiJumpCont);
    SP -= 2;
    JUMP(chiToCont(SP[0]));
}

CONT(chiSetupEntryPointsCont,, .size = 1) {
    PROLOGUE_LIMITS(chiSetupEntryPointsCont);
    --SP;

    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    const Chili mod = A(0);

    // disable scheduling
    if (!chiSuccess(mod)) {
        ChiRuntimeOption* opt = &rt->option;
        opt->interval = (ChiMillis){0};
        opt->blackhole = CHI_BH_NONE;
        opt->processors = 1;
    } else {
        ChiEntryPoint* ep = &rt->entryPoint;

        chiUnroot(rt, ep->exit); // Remove original roots, registered in setup()
        chiUnroot(rt, ep->unhandled);
        chiUnroot(rt, ep->interrupt);

        ep->exit      = chiRoot(rt, IDX(mod, (size_t)z_System_2fEntryPoints_z_exit));
        ep->unhandled = chiRoot(rt, IDX(mod, (size_t)z_System_2fEntryPoints_z_unhandled));
        ep->interrupt = chiRoot(rt, IDX(mod, (size_t)z_System_2fEntryPoints_z_interrupt));
        ep->par       = chiRoot(rt, IDX(mod, (size_t)z_System_2fEntryPoints_z_par));
        ep->scheduler = chiRoot(rt, IDX(mod, (size_t)z_System_2fEntryPoints_z_scheduler));

        CHI_ASSERT(chiFnArity(ep->exit)      <= 1);
        CHI_ASSERT(chiFnArity(ep->unhandled) <= 2);
        CHI_ASSERT(chiFnArity(ep->interrupt) <= 1);
        CHI_ASSERT(chiFnArity(ep->par)       <= 1);
        CHI_ASSERT(chiFnArity(ep->scheduler) <= 2);
    }

    RET(CHI_FALSE);
}

CONT(chiStartupEndCont,, .size = 1) {
    PROLOGUE_LIMITS(chiStartupEndCont);
    --SP;
    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    CHI_EVENT0(rt, STARTUP_END);
    RET(CHI_FALSE);
}

CONT(chiSwitchThread,, .na = 1) {
    PROLOGUE_LIMITS(chiSwitchThread);

    Chili result = A(1);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(chiIdentical(proc->thread, proc->schedulerThread));

    CHI_THREAD_EVENT(END, proc);
    chiStackDeactivateReset(proc);

    proc->thread = IDX(result, 0);
    CHI_THREAD_EVENT(BEGIN, proc);
    chiStackActivate(proc);
    RESTORE_STACK_REGS;
    ASSERT_INVARIANTS;

    // Throw asynchronous exception
    Chili just = IDX(result, 1);
    if (chiTrue(just))
        THROW(IDX(just, 0));

    ENTER;
}

STATIC_CONT(protectEnd) {
    PROLOGUE_LIMITS(protectEnd);
    RET(A(0));
}

CONT(chiProtectEnd) {
    _PROLOGUE; // invalid stack frame, use _PROLOGUE which does not have a tracepoint
    CHI_ASSERT(chiUnboxed(A(0)) || chiPinned(A(0)));
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiProcessorProtectEnd(proc);
    RESTORE_STACK_REGS;
    RESTORE_HEAP_REGS;
    if (proc->trigger.interrupt) // restore interrupt trigger
        HLRW = 0;
    ASSERT_INVARIANTS;
    KNOWN_JUMP_NOTRACE(protectEnd); // invalid stack frame, use NOTRACE
}

/**
 * chiInterrupt is the central entry point to the runtime.
 * In particular it acts as a safe point for garbage collection.
 *
 * * Stack resizing
 * * Supplying fresh blocks for bump allocation, see ::chiNurseryGrow.
 * * Entering the scheduler when time elapsed or thread yields, see ::chiThreadScheduler.
 * * Handling stack dump signals, see ::chiStackDump.
 * * Stopping the world via ::chiProcessorSync,
 *   which in turn is responsible for running GC slices
 */
CONT(chiInterrupt) {
    PROLOGUE(chiInterrupt); // invariant violated since SL is modified

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;

    // Poll processor tick on systems without interrupt
    const ChiRuntimeOption* opt = &proc->rt->option;
    if (!CHI_SYSTEM_HAS_INTERRUPT) {
        if (chiMillisZero(opt->interval)) {
            AUX.EN = ~0U;
        } else {
            AUX.EN = opt->evalCount;
            ChiNanos time = chiClock(CHI_CLOCK_REAL_FINE);
            if (chiNanosLess(proc->trigger.nextTick, time)) {
                proc->trigger.interrupt = proc->trigger.tick = true;
                proc->trigger.nextTick = chiNanosAdd(time, chiMillisToNanos(opt->interval));
            }
        }
    }

    bool interrupt = proc->trigger.interrupt;
    proc->trigger.interrupt = false;

    // limits were used to pass arguments.
    // thus we have to restore the old limits.
    ChiWord* newHL = HL;
    Chili* newSL = SL;
    HLRW = chiNurseryLimit(&proc->nursery);
    SLRW = getStackLimit(proc);
    ASSERT_INVARIANTS;

    if (!CHI_TRACEPOINTS_ENABLED)
        CHI_TT(&proc->worker, .name = "unknown native", .thread = true);

    // Grow heap if requested
    if (newHL > chiNurseryLimit(&proc->nursery)) {
        chiNurseryGrow(&proc->nursery);
        RESTORE_HEAP_REGS;
        ASSERT_INVARIANTS;
    }

    uint32_t ts = chiTag(chiThreadState(proc->thread));
    bool schedule = ts != CHI_TS_RUNNING;
    if (proc->trigger.tick) {
        CHI_EVENT0(proc, PROC_TICK);
        proc->trigger.tick = false;
        schedule = !chiIdentical(proc->thread, proc->schedulerThread) && chiTrue(proc->rt->entryPoint.scheduler);
    }

    // Reset stack or grow stack if requested
    if (ts == CHI_TS_TERMINATED) {
        chiStackDeactivateReset(proc);
    } else {
        // Ensure that there is enough stack space for restore frame
        // restore frame: cont, a0, ..., a[na], size, chiEnterCont
        uint32_t na = chiFrameArgs(SP, A(0), NA);
        newSL = CHI_MAX(newSL, SP + na + 4);

        Chili newStack =
            newSL != getStackLimit(proc) && // resize requested
            newSL > getStackBase(proc) + proc->rt->option.stack.max && // request to large
            !chiThreadExceptionBlock(proc->thread)
            ? CHI_FAIL : chiStackTryResize(proc, SP, newSL);

        if (!chiSuccess(newStack))
            CHI_THROW_ASYNC_EX(proc, CHI_STACK_OVERFLOW);

        if (!chiIdentical(newStack, chiThreadStack(proc->thread))) {
            chiStackDeactivateReset(proc);
            chiWriteField(proc, proc->thread, &chiToThread(proc->thread)->stack, newStack);
            chiStackActivate(proc);
            CHI_TT(&proc->worker, .cont = chiToCont(SP[0]), .name = "stack resize", .thread = true);
        }

        // Restore stack registers, stack might have been resized (Even if it is the same object!)
        RESTORE_STACK_REGS;
        ASSERT_INVARIANTS;

        if (!interrupt) {
            CHI_ASSERT(chiTag(chiThreadState(proc->thread)) == CHI_TS_RUNNING);
            JUMP_FN(SP[0]);
        }

        // Create restore frame, save thread
        CHI_BOUNDED(na, CHI_AMAX);
        for (uint32_t i = 0; i <= na; ++i)
            SP[i + 1] = A(i);
        SP[na + 2] = chiFromUnboxed((ChiWord)na + 4);
        SP[na + 3] = chiFromCont(&chiEnterCont);
        SP += na + 4;
        ASSERT_INVARIANTS;
        if (schedule)
            chiStackBlackhole(proc);
        chiStackDeactivate(proc, SP);
    }

    CHI_THREAD_EVENT(END, proc);
    chiNurserySetPtr(&proc->nursery, HP);

    if (!chiProcessorSync(proc))
        KNOWN_JUMP_NOTRACE(exitHandler);

    RESTORE_HEAP_REGS;

    if (schedule)
        chiThreadScheduler(proc);

    chiStackActivate(proc);
    RESTORE_STACK_REGS;
    ASSERT_INVARIANTS;

    CHI_THREAD_EVENT(BEGIN, proc);
    CHI_ASSERT(chiTag(chiThreadState(proc->thread)) == CHI_TS_RUNNING);
    ENTER;
}

CONT(chiForce,, .na = 1) {
    PROLOGUE_LIMITS(chiForce, .stack=2);

    Chili thunk = A(0), clos = A(1);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (CHI_UNLIKELY(proc->rt->option.blackhole == CHI_BH_EAGER))
        chiThunkCas(proc, thunk, clos, proc->rt->blackhole);

    SP[0] = thunk;
    SP[1] = chiFromCont(&updateCont);
    SP += 2;

    A(0) = clos;
    JUMP_FN(IDX(clos, 0));
}

CONT(chiBlackhole) {
    PROLOGUE_LIMITS(chiBlackhole, .heap=1);

    Chili thunk = SP[-2], val;
    if (!chiThunkForced(thunk, &val)) {
        ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
        CHI_EVENT0(proc, THREAD_BLACKHOLE);
        Chili c = NEW(CHI_TAG(CHI_TS_WAIT_BLACKHOLE), 1);
        IDX(c, 0) = thunk;
        chiThreadSetStateField(proc, proc->thread, c);
        JUMP_TO_INTERRUPT(chiBlackhole);
    }
    SP -= 2;
    RET(val);
}

CONT(chiShowAsyncException,, .na = 1) {
    PROLOGUE_LIMITS(chiShowAsyncException);
    static const char* const asyncExceptionName[] =
        { "HeapOverflow",
          "StackOverflow",
          "ThreadInterrupt",
          "UserInterrupt" };
    RET(chiStringNew(asyncExceptionName[chiToUInt32(A(1))]));
}

CONT(chiStackTrace) {
    PROLOGUE_LIMITS(chiStackTrace);
    RET(chiStackGetTrace(CHI_CURRENT_PROCESSOR, SP));
}

CONT(chiIdentity,, .na = 1) {
    PROLOGUE_LIMITS(chiIdentity);
    RET(A(1));
}

STATIC_CONT(showUnhandledCont,, .size = 2) {
    PROLOGUE_INVARIANTS(showUnhandledCont);
    SP -= 2;
    Chili name = SP[0], except = A(0);
    chiSinkFmt(chiStderr, "Unhandled exception occurred:\n%S: %S\n",
               chiStringRef(&name), chiStringRef(&except));
    A(0) = CHI_TRUE;
    KNOWN_JUMP(chiExit);
}

CONT(chiEntryPointUnhandledDefault,, .na = 2) {
    PROLOGUE_LIMITS(chiEntryPointUnhandledDefault, .stack = 2);

    const Chili except = A(1);
    const Chili info = IDX(except, 0);
    const Chili identifier = IDX(info, 0);
    const Chili name = IDX(identifier, 1);

    SP[0] = name;
    SP[1] = chiFromCont(&showUnhandledCont);
    SP += 2;

    A(0) = IDX(info, 1);
    A(1) = IDX(except, 1);
    APP(1);
}

CONT(chiThrow) {
    PROLOGUE_INVARIANTS(chiThrow);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;

    const Chili except = A(0);
    const Chili info = IDX(except, 0);
    const Chili identifier = IDX(info, 0);
    const Chili name = IDX(identifier, 1);

    // Update the stack pointer to prepare for stack dump
    getStack(proc)->sp = SP;

    Chili* catchFrame = chiStackUnwind(proc, except);
    if (catchFrame) {
        STACK_DUMP(proc, EXCEPTION_HANDLED, .name = chiStringRef(&name));
        SP = catchFrame;
        A(0) = *SP;
        A(1) = except;
        APP(1);
    }

    STACK_DUMP(proc, EXCEPTION_UNHANDLED, .name = chiStringRef(&name));

    chiThreadSetStateField(proc, proc->thread, chiNewEmpty(CHI_TAG(CHI_TS_RUNNING)));
    SP = getStackBase(proc);
    A(0) = proc->rt->entryPoint.unhandled;
    A(1) = except;
    A(2) = CHI_FALSE;
    APP(2);
}

CONT(chiThunkThrow) {
    PROLOGUE_LIMITS(chiThunkThrow);
    THROW(IDX(A(0), 1));
}

CONT(chiHeapOverflow) {
    PROLOGUE_LIMITS(chiHeapOverflow);
    CHI_THROW_ASYNC_EX(CHI_CURRENT_PROCESSOR, CHI_HEAP_OVERFLOW);
}

CONT(chiCatch,, .na = 1) {
    PROLOGUE_LIMITS(chiCatch, .stack=3);

    SP[0] = A(1);
    SP[1] = chiFromCont(&catchCont);
    SP += 2;

    A(1) = CHI_FALSE;
    APP(1);
}

CONT(chiThreadYield) {
    PROLOGUE_LIMITS(chiThreadYield);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (!chiTrue(proc->rt->entryPoint.scheduler)) {
        chiWarn("Thread.yield called with uninitialized scheduler");
        A(0) = CHI_FALSE;
        KNOWN_JUMP(chiExit);
    }
    proc->trigger.tick = true;
    JUMP_TO_INTERRUPT(returnUnit);
}

CONT(chiGCRun) {
    PROLOGUE_INVARIANTS(chiGCRun);
    chiGCTrigger(CHI_CURRENT_RUNTIME, CHI_GC_REQUESTOR_USER, CHI_GC_TRIGGER_FULL, CHI_NODUMP);
    JUMP_TO_INTERRUPT(returnUnit);
}

CONT(chiProcessorStart) {
    PROLOGUE_INVARIANTS(chiProcessorStart);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    uint32_t pid = chiProcessorTryStart(proc->rt);
    if (!pid)
        CHI_THROW_RUNTIME_EX(proc, STATIC_STRING("Failed to start processor"));

    RET(chiFromUnboxed(pid));
}

#define SUSPEND_END                                             \
    ({                                                          \
        ChiProcessor* proc = CHI_CURRENT_PROCESSOR;             \
        CHI_TT(&proc->worker, .name = "runtime;suspend");       \
        CHI_EVENT0(proc, PROC_SUSPEND_END);                     \
        CHI_EVENT0(proc, PROC_RUN_BEGIN);                       \
        CHI_THREAD_EVENT(BEGIN, proc);                          \
    })

#define SUSPEND_BEGIN                           \
    ({                                          \
        ChiProcessor* proc = CHI_CURRENT_PROCESSOR;     \
        CHI_THREAD_EVENT(END, proc);            \
        CHI_EVENT0(proc, PROC_RUN_END);         \
        CHI_EVENT0(proc, PROC_SUSPEND_BEGIN);   \
    })

#ifdef CHI_STANDALONE_WASM
// HACK: Works only with *_tls callconv, since the regstore must be global
// TODO: clang -mtail-call will also define __wasm_tail_call__
#define CHECK_CALLCONV_trampoline_tls 1
#ifdef __wasm_tail_call__
#  define CHECK_CALLCONV_tail_tls_arg8 1
#endif
_Static_assert(CHI_CAT(CHECK_CALLCONV_,CHI_CALLCONV), "Invalid callconv for wasm");

_Noreturn void chiWasmContinue(void) {
    _PROLOGUE;
    SUSPEND_END;
    ASSERT_INVARIANTS;
    FIRST_JUMP(chiToCont(SP[-1]));
}

CONT(chiProcessorSuspend) {
    PROLOGUE_INVARIANTS(chiProcessorSuspend);
    SUSPEND_BEGIN;
    wasm_throw(&(wasm_exception_t){ WASM_SUSPEND, chiToUInt32(A(0)) });
}
#else
CONT(chiProcessorSuspend) {
    PROLOGUE_INVARIANTS(chiProcessorSuspend);

    PROTECT_BEGIN;
    SUSPEND_BEGIN;

    {
        ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
        CHI_LOCK(&proc->suspend.mutex);
        proc->suspend.timeout = (ChiMillis){chiToUInt32(A(0))};
        while (!chiMillisZero(proc->suspend.timeout)) {
            ChiMillis waited = chiNanosToMillis(chiCondTimedWait(&proc->suspend.cond,
                                                                 &proc->suspend.mutex,
                                                                 chiMillisToNanos(proc->suspend.timeout)));

            proc->suspend.timeout = chiMillisDelta(proc->suspend.timeout, waited);
        }
    }

    SUSPEND_END;
    PROTECT_END(CHI_FALSE);
}
#endif

CONT(chiPar) {
    PROLOGUE_LIMITS(chiPar);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_EVENT0(proc, PAR);
    A(1) = A(0);
    A(0) = proc->rt->entryPoint.par;
    APP(1);
}

CONT(chiOverApp,, .size = CHI_VASIZE) {
    PROLOGUE_LIMITS(chiOverApp);

    const size_t size = (size_t)chiToUnboxed(SP[-2]) - 2;
    SP -= size + 2;

    CHI_BOUNDED(size, CHI_AMAX);
    for (size_t i = 0; i < size; ++i)
        A(i + 1) = SP[i];

    APP(size);
}

CONT(chiPartialNofM,, .na = CHI_VAARGS) {
    PROLOGUE_LIMITS(chiPartialNofM);

    Chili clos = A(0);
    const size_t arity = chiFnArity(clos), args = chiSize(clos) - 2;

    CHI_BOUNDED(arity, CHI_AMAX);
    for (size_t i = arity; i > 0; --i)
        A(args + i) = A(i);

    CHI_BOUNDED(args, CHI_AMAX - 1);
    for (size_t i = 0; i <= args; ++i)
        A(i) = IDX(clos, i + 1);

    JUMP_FN0;
}

CONT(chiAppN,, .type = CHI_FRAME_APPN, .na = CHI_VAARGS) {
    PROLOGUE_INVARIANTS(chiAppN);

    const size_t arity = chiFnArity(A(0));
    CHI_ASSERT(arity < CHI_AMAX);

    if (NA < arity) {
        LIMITS(.heap=(size_t)NA + 2);
        PARTIAL(.args=NA, .arity=arity);
    }

    if (NA > arity) {
        LIMITS(.stack=(size_t)NA - arity + 2);
        OVERAPP(.args=NA, .arity=arity);
    }

    LIMITS();
    JUMP_FN(IDX(A(0), 0));
}

_Noreturn void chiEnter(void) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CALLCONV_INIT;
    proc->trigger.hl = &HLRW;
    proc->worker.tp = &AUX.TP;
    chiHookRun(&proc->worker, CHI_HOOK_PROC_START);
    if (chiTag(chiThreadState(proc->thread)) != CHI_TS_RUNNING)
        chiThreadScheduler(proc);
    CHI_EVENT0(proc, PROC_RUN_BEGIN);
    CHI_THREAD_EVENT(BEGIN, proc);
    chiStackActivate(proc);
    RESTORE_STACK_REGS;
    RESTORE_HEAP_REGS;
    ASSERT_INVARIANTS;
    if (!CHI_SYSTEM_HAS_INTERRUPT)
        AUX.EN = proc->rt->option.evalCount;
    FIRST_JUMP(chiToCont(SP[-1]));
}

#define DEFINE_APP(a)                                   \
    CONT(chiApp##a,, .na = a) {                         \
        PROLOGUE_INVARIANTS(chiApp##a);                 \
                                                        \
        const size_t arity = chiFnArity(A(0));          \
        CHI_ASSERT(arity < CHI_AMAX);                   \
                                                        \
        if (a < arity) {                                \
            /*LIMITS(.heap=a + 2);*/                    \
            LIMITS(.heap=CHI_AMAX + 2);                 \
            PARTIAL(.args=a, .arity=arity);             \
        }                                               \
                                                        \
        if (a > arity) {                                \
            /*LIMITS(.stack=a - arity + 2);*/           \
            LIMITS(.stack=CHI_AMAX + 2);                \
            OVERAPP(.args=a, .arity=arity);             \
        }                                               \
                                                        \
        LIMITS();                                       \
        JUMP_FN0;                                       \
    }

DEFINE_APP(1)
DEFINE_APP(2)
DEFINE_APP(3)
DEFINE_APP(4)

#define DEFINE_PARTIAL(arity, args)                             \
    CONT(chiPartial##args##of##arity,, .na = arity - args) {    \
        PROLOGUE_LIMITS(chiPartial##args##of##arity);           \
        Chili clos = A(0);                                      \
        CHI_ASSERT(chiFnArity(clos) == arity - args);          \
        CHI_ASSERT(chiSize(clos) - 2 == args);                  \
                                                                \
        for (size_t i = arity - args; i > 0; --i)               \
            A(args + i) = A(i);                                 \
                                                                \
        for (size_t i = 0; i < args + 1; ++i)                   \
            A(i) = IDX(clos, i + 1);                            \
                                                                \
        JUMP_FN0;                                               \
    }

DEFINE_PARTIAL(2, 1)
DEFINE_PARTIAL(3, 1)
DEFINE_PARTIAL(3, 2)
DEFINE_PARTIAL(4, 1)
DEFINE_PARTIAL(4, 2)
DEFINE_PARTIAL(4, 3)
