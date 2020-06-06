#include <chili/cont.h>
#include <stdlib.h>
#include "barrier.h"
#include "error.h"
#include "event.h"
#include "new.h"
#include "sink.h"
#include "stack.h"
#include "tracepoint.h"

#define ASSERT_INVARIANTS                                               \
    ({                                                                  \
        CHI_ASSERT(chiIdentical(AUX.THREAD, CHI_CURRENT_PROCESSOR->thread)); \
        CHI_ASSERT(chiIdentical(AUX.PROC, chiFromPtr(CHI_CURRENT_PROCESSOR))); \
        CHI_ASSERT(chiIdentical(AUX.PLOCAL, CHI_CURRENT_PROCESSOR->local)); \
        CHI_ASSERT(SP >= getStack(AUX.THREAD)->base);                   \
        CHI_ASSERT(SP <= getStackLimit(AUX.THREAD));                        \
        CHI_ASSERT(SL == getStackLimit(AUX.THREAD));                        \
        CHI_ASSERT(HP >= chiLocalHeapBumpBase(&CHI_CURRENT_PROCESSOR->heap)); \
        CHI_ASSERT(HP <= chiLocalHeapBumpLimit(&CHI_CURRENT_PROCESSOR->heap)); \
        chiStackDebugWalk(chiThreadStack(CHI_CURRENT_PROCESSOR->thread), SP, SL); \
    })

#define PROLOGUE_INVARIANTS(c) PROLOGUE(c); ASSERT_INVARIANTS

#define RESTORE_STACK_REGS                      \
    ({                                          \
        CHI_ASSERT(chiIdentical(proc->thread, AUX.THREAD)); \
        SP = getStack(AUX.THREAD)->sp;               \
        SLRW = getStackLimit(AUX.THREAD);            \
        CHI_ASSERT(SP >= getStack(AUX.THREAD)->base);   \
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

enum {
    EP_BLACKHOLE,
    EP_NOTIFYINTERRUPT,
    EP_START,
    EP_TIMERINTERRUPT,
    EP_UNHANDLED,
    EP_USERINTERRUPT,
};

CHI_INL ChiStack* getStack(Chili thread) {
    return chiToStack(chiThreadStack(thread));
}

CHI_INL Chili* getStackLimit(Chili thread) {
    return chiStackLimit(chiThreadStack(thread));
}

CONT(chiThunkUpdateCont, .na = 1, .size = 2) {
    PROLOGUE_INVARIANTS(chiThunkUpdateCont);
    Chili val = AGET(0);
    UNDEF_ARGS(0);
    SP -= 2;
    Chili thunk = SP[0];
    ChiThunk* obj = chiToThunk(thunk);
    chiFieldWrite(CHI_CURRENT_PROCESSOR, thunk, &obj->val, val, CHI_BARRIER_GENERIC);
    ASET(0, val);
    RET;
}

INTERN_CONT(chiThunkForward, .na = 1) {
    PROLOGUE_INVARIANTS(chiThunkForward);
    Chili thunk = AGET(0);
    UNDEF_ARGS(0);
    ChiThunk* obj = chiToThunk(thunk);
    Chili fw = chiFieldRead(&obj->val);
    chiFieldWrite(CHI_CURRENT_PROCESSOR, thunk, &obj->val, thunk, CHI_BARRIER_GENERIC);
    FORCE(fw, RET);
}

STATIC_CONT(blackholeCont, .size = 1) {
    PROLOGUE_INVARIANTS(blackholeCont);
    UNDEF_ARGS(0);
    --SP;
    Chili thunk = SP[-2];
    ASET(0, thunk);
    KNOWN_JUMP(chiThunkBlackhole);
}

STATIC_CONT(thunkReturn, .na = 1) {
    PROLOGUE_INVARIANTS(thunkReturn);
    Chili thunk = AGET(0);
    UNDEF_ARGS(0);
    ASET(0, chiFieldRead(&chiToThunk(thunk)->val));
    RET;
}

CONT(chiThunkBlackhole, .na = 1) {
    PROLOGUE_INVARIANTS(chiThunkBlackhole);
    UNDEF_ARGS(1);
    Chili thunk = AGET(0), val;
    if (chiThunkForced(thunk, &val)) {
        // Thunk forced twice, assume more forces?
        // TODO: Investigate if we should better do this directly in chiThunkUpdateCont!
        chiFieldInit(&chiToThunk(thunk)->cont, chiFromCont(&thunkReturn));
        ASET(0, val);
        RET;
    }
    LIMITS_PROC(.stack = 1);
    *SP++ = chiFromCont(&blackholeCont);
    ASET(0, chiIdx(CHI_CURRENT_RUNTIME->entryPoints, EP_BLACKHOLE));
    ASET(1, CHI_FALSE);
    APP(1);
}

INTERN_CONT(chiExceptionCatchCont, .na = 1, .size = 2) {
    PROLOGUE_INVARIANTS(chiExceptionCatchCont);
    Chili res = AGET(0);
    UNDEF_ARGS(0);
    SP -= 2;
    ASET(0, res);
    RET;
}

#ifdef CHI_STANDALONE_WASM
// TODO: Wasm linker does not yet support --defsym
// see also main/wasm.c
extern int32_t wasmMainIdx(void);
#  define MAIN_IDX wasmMainIdx()
#else
extern int32_t main_z_Main;
#  define MAIN_IDX main_z_Main
#endif

INTERN_CONT(chiRunMainCont, .na = 1, .size = 1) {
    PROLOGUE_INVARIANTS(chiRunMainCont);
    Chili mainMod = AGET(0);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (MAIN_IDX < -1) {
        ASET(0, chiStringNew(proc, "No main function"));
        KNOWN_JUMP(chiThrowRuntimeException);
    }

    --SP;

    ASET(0, chiIdx(proc->rt->entryPoints, EP_START));
    ASET(1, MAIN_IDX >= 0 ? chiIdx(mainMod, (size_t)MAIN_IDX) : mainMod);
    ASET(2, CHI_FALSE);
    APP(2);
}

INTERN_CONT(chiRestoreCont, .size = CHI_VASIZE) {
    PROLOGUE_INVARIANTS(chiRestoreCont);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;

    if (chiThreadInterruptible(proc->thread) && chiTrue(rt->entryPoints)) {
        int32_t idx = -1;
        if (CHI_SYSTEM_HAS_INTERRUPT && chiTriggered(&proc->trigger.userInterrupt)) {
            chiTrigger(&proc->trigger.userInterrupt, false);
            idx = EP_USERINTERRUPT;
        } else if (chiTriggered(&proc->trigger.notifyInterrupt)) {
            chiTrigger(&proc->trigger.notifyInterrupt, false);
            idx = EP_NOTIFYINTERRUPT;
        } else if (chiTriggered(&proc->trigger.timerInterrupt)) {
            chiTrigger(&proc->trigger.timerInterrupt, false);
            idx = EP_TIMERINTERRUPT;
        }
        if (idx >= 0) {
            ASET(0, chiIdx(rt->entryPoints, (size_t)idx));
            ASET(1, CHI_FALSE);
            APP(1);
        }
    }

    size_t size = (size_t)chiToUnboxed(SP[-2]);
    SP -= size;

    size -= 3;
    AUX.VAARGS = (uint8_t)size;
    for (size_t i = 0; i < size; ++i)
        ASET(i, SP[i + 1]);

    JUMP_FN(SP[0]);
}

INTERN_CONT(chiJumpCont, .size = 2) {
    PROLOGUE_INVARIANTS(chiJumpCont);
    UNDEF_ARGS(0);
    SP -= 2;
    JUMP_FN(SP[0]);
}

INTERN_CONT(chiSetupEntryPointsCont, .na = 1, .size = 1) {
    PROLOGUE_INVARIANTS(chiSetupEntryPointsCont);
    Chili mod = AGET(0);
    UNDEF_ARGS(0);
    --SP;
    if (!chiSuccess(mod))
        RET;
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;
    rt->entryPoints = mod;
    chiPromoteShared(proc, &rt->entryPoints);
    chiGCRoot(rt, rt->entryPoints);
    RET;
}

INTERN_CONT(chiStartupEndCont, .size = 1) {
    PROLOGUE_INVARIANTS(chiStartupEndCont);
    UNDEF_ARGS(0);
    --SP;
    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    chiEvent0(rt, STARTUP_END);
    RET;
}

STATIC_CONT(showUnhandledCont, .na = 1, .size = 2) {
    PROLOGUE_INVARIANTS(showUnhandledCont);
    Chili except = AGET(0);
    UNDEF_ARGS(0);
    SP -= 2;
    Chili name = SP[0];
    chiSinkFmt(chiStderr, "Unhandled exception occurred:\n%S: %S\n",
               chiStringRef(&name), chiStringRef(&except));
    chiExit(1);
    RET;
}

STATIC_CONT(unhandledDefault, .na = 1) {
    PROLOGUE_INVARIANTS(unhandledDefault);
    LIMITS_PROC(.stack = 2);
    const Chili except = AGET(0);
    UNDEF_ARGS(0);

    const Chili info = chiIdx(except, 0);
    const Chili identifier = chiIdx(info, 0);
    const Chili name = chiIdx(identifier, 1);

    SP[0] = name;
    SP[1] = chiFromCont(&showUnhandledCont);
    SP += 2;

    ASET(0, chiIdx(info, 1));
    ASET(1, chiIdx(except, 1));
    APP(1);
}

CONT(chiExceptionThrow, .na = 1) {
    PROLOGUE_INVARIANTS(chiExceptionThrow);
    const Chili except = AGET(0);
    UNDEF_ARGS(0);

    const Chili info = chiIdx(except, 0);
    const Chili identifier = chiIdx(info, 0);
    const Chili name = chiIdx(identifier, 1);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (chiEventEnabled(proc, EXCEPTION)) {
        CHI_STRING_SINK(sink);
        chiStackDump(sink, proc, SP);
        chiEvent(proc, EXCEPTION, .trace = chiSinkString(sink), .name = chiStringRef(&name));
    }

    bool handlerFound = chiStackUnwind(proc, SP);
    RESTORE_STACK_REGS;
    ASSERT_INVARIANTS;

    if (handlerFound) {
        ASET(0, *SP);
        ASET(1, except);
        APP(1);
    }

    chiThreadSetInterruptible(proc, false);

    if (!chiTrue(proc->rt->entryPoints)) {
        ASET(0, except);
        KNOWN_JUMP(unhandledDefault);
    }

    ASET(0, chiIdx(proc->rt->entryPoints, EP_UNHANDLED));
    ASET(1, except);
    ASET(2, CHI_FALSE);
    APP(2);
}

CONT(chiThreadSwitch, .na = 1) {
    PROLOGUE_INVARIANTS(chiThreadSwitch);
    Chili next = AGET(0);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiEvent(proc, THREAD_SWITCH, .nextTid = chiThreadId(next));

    DEACTIVATE_THREAD;
    proc->thread = next;
    ACTIVATE_THREAD;

    ASET(0, CHI_FALSE);
    RET;
}

CONT(chiProtectEnd, .na = 1) {
    _PROLOGUE; // invalid stack frame, use _PROLOGUE which does not have a tracepoint
    Chili res = AGET(0);
    UNDEF_ARGS(0);

    CHI_ASSERT(!chiRef(res) || chiGen(res) == CHI_GEN_MAJOR);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiProcessorProtectEnd(proc);
    RESTORE_HEAP_REGS;
    ACTIVATE_THREAD;

    const ChiCont _chiCurrentCont = &chiProtectEnd;
    CHI_NOWARN_UNUSED(_chiCurrentCont);
    TRACE_FFI(protect);

    ASET(0, res);
    RET;
}

#if !CHI_SYSTEM_HAS_INTERRUPT
static void pollTimer(ChiProcessor* proc) {
    ChiNanos time = chiClockFine();
    if (chiNanosLess(proc->trigger.lastTimerInterrupt, time)) {
        chiTrigger(&proc->trigger.interrupt, true);
        chiTrigger(&proc->trigger.timerInterrupt, true);
        proc->trigger.lastTimerInterrupt = chiNanosAdd(time, chiMillisToNanos(proc->rt->option.interval));
    }
}
#endif

STATIC_CONT(showAsyncException, .na = 2) {
    PROLOGUE_INVARIANTS(showAsyncException);
    Chili except = AGET(1);
    UNDEF_ARGS(0);
    static const char* const asyncExceptionName[] =
        { "HeapOverflow",
          "StackOverflow",
          "ThreadInterrupt",
          "UserInterrupt" };
    ASET(0, chiStringFromRef(chiStringRef(asyncExceptionName[chiToUInt32(except)])));
    RET;
}

STATIC_CONT(throwAsyncException, .na = 1) {
    PROLOGUE_INVARIANTS(throwAsyncException);
    LIMITS_PROC(.heap = 1 + 2 + 2 + 3);
    Chili tag = AGET(0);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;

    NEW(show, CHI_FN(1), 1);
    NEW_INIT(show, 0, chiFromCont(&showAsyncException));

    Chili id = chiStringNew(proc, "AsyncException"); // TODO use uint128 identifier
    NEW(identifier, CHI_TAG(0), 2);
    NEW_INIT(identifier, 0, id);
    NEW_INIT(identifier, 1, id);

    NEW(info, CHI_TAG(0), 2);
    NEW_INIT(info, 0, identifier);
    NEW_INIT(info, 1, show);

    NEW(except, CHI_TAG(0), 3);
    NEW_INIT(except, 0, info);
    NEW_INIT(except, 1, tag);
    NEW_INIT(except, 2, chiStackGetTrace(proc, SP));

    ASET(0, except);
    KNOWN_JUMP(chiExceptionThrow);
}

STATIC_CONT(identity, .na = 2) {
    PROLOGUE_INVARIANTS(identity);
    Chili res = AGET(1);
    UNDEF_ARGS(0);
    ASET(0, res);
    RET;
}

INTERN_CONT(chiThrowRuntimeException, .na = 1) {
    PROLOGUE_INVARIANTS(chiThrowRuntimeException);
    LIMITS_PROC(.heap = 1 + 2 + 2 + 3);
    Chili msg = AGET(0);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;

    NEW(show, CHI_FN(1), 1);
    NEW_INIT(show, 0, chiFromCont(&identity));

    Chili id = chiStringNew(proc, "RuntimeException"); // TODO use uint128 identifier
    NEW(identifier, CHI_TAG(0), 2);
    NEW_INIT(identifier, 0, id);
    NEW_INIT(identifier, 1, id);

    NEW(info, CHI_TAG(0), 2);
    NEW_INIT(info, 0, identifier);
    NEW_INIT(info, 1, show);

    NEW(except, CHI_TAG(0), 3);
    NEW_INIT(except, 0, info);
    NEW_INIT(except, 1, msg);
    NEW_INIT(except, 2, chiStackGetTrace(proc, SP));

    ASET(0, except);
    KNOWN_JUMP(chiExceptionThrow);
}

INTERN_CONT(chiStackGrowCont, .size = 2, .na = CHI_VAARGS) {
    PROLOGUE_INVARIANTS(chiStackGrowCont);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(SP == getStack(proc->thread)->base + 2);
    chiStackShrink(proc);

    RESTORE_STACK_REGS;
    ASSERT_INVARIANTS;

    RET;
}

/**
 * chiYield is the central entry point to the runtime.
 * In particular it acts as a safe point for garbage collection.
 *
 * * Stack resizing
 * * Calling the timer interrupt handler
 * * Supplying fresh blocks for bump allocation, see ::chiLocalHeapBumpGrow.
 * * Handling stack dump signals, see ::chiStackDump.
 * * Handling processor service requests via ::chiProcessorService
 */
INTERN_CONT(chiYield, .na = CHI_VAARGS) {
    PROLOGUE(chiYield); // invariant violated since SL is modified

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;

    CHI_UNLESS(CHI_SYSTEM_HAS_INTERRUPT, pollTimer(proc);)

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

    /* Ensure that there is enough stack space for restore frame
     * restore frame: cont, a0, ..., a[na], size, chiRestoreCont
     *
     * For CHI_VAARGS frames the number of arguments must be stored in AUX.VAARGS!
     */
    uint32_t na = chiContInfo(chiToCont(AUX.YIELD))->na;
    if (na == CHI_VAARGS)
        na = AUX.VAARGS;
    CHI_SETMAX(&newSL, SP + na + 3);

    if (newSL > SL) {
        uint32_t ex = chiStackTryGrow(proc, SP, newSL);
        if (ex != UINT32_MAX) {
            ASET(0, chiFromUInt32(ex));
            KNOWN_JUMP(throwAsyncException);
        }

        RESTORE_STACK_REGS;
        ASSERT_INVARIANTS;
    }

    if (!chiTriggered(&proc->trigger.interrupt))
        JUMP_FN(AUX.YIELD);

    // Create restore frame, save thread
    CHI_ASSUME(na < CHI_AMAX);
    SP[0] = AUX.YIELD;
    for (uint32_t i = 0; i < na; ++i)
        SP[i + 1] = AGET(i);
    SP[na + 1] = chiFromUnboxed((ChiWord)na + 3);
    SP[na + 2] = chiFromCont(&chiRestoreCont);
    SP += na + 3;
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

CONT(chiYieldClos, .na = CHI_VAARGS) {
    PROLOGUE(chiYieldClos); // invariant violated since SL is modified
    AUX.YIELD = chiIdx(AGET(0), 0);
    KNOWN_JUMP(chiYield);
}

CONT(chiYieldCont, .na = CHI_VAARGS) {
    PROLOGUE(chiYieldCont); // invariant violated since SL is modified
    AUX.YIELD = SP[-1];
    KNOWN_JUMP(chiYield);
}

CONT(chiStackTrace) {
    PROLOGUE_INVARIANTS(chiStackTrace);
    UNDEF_ARGS(0);
    ASET(0, chiStackGetTrace(CHI_CURRENT_PROCESSOR, SP));
    RET;
}

CONT(chiHeapOverflow) {
    PROLOGUE_INVARIANTS(chiHeapOverflow);
    UNDEF_ARGS(0);
    ASET(0, chiFromUInt32(CHI_HEAP_OVERFLOW));
    KNOWN_JUMP(throwAsyncException);
}

CONT(chiExceptionCatch, .na = 2) {
    PROLOGUE_INVARIANTS(chiExceptionCatch);
    LIMITS_PROC(.stack=3);
    Chili run = AGET(0), handler = AGET(1);
    UNDEF_ARGS(0);

    SP[0] = handler;
    SP[1] = chiFromCont(&chiExceptionCatchCont);
    SP += 2;

    ASET(0, run);
    ASET(1, CHI_FALSE);
    APP(1);
}

STATIC_CONT(returnUnit) {
    PROLOGUE_INVARIANTS(returnUnit);
    ASET(0, CHI_FALSE);
    RET;
}

CONT(chiPromoteAll) {
    PROLOGUE_INVARIANTS(chiPromoteAll);
    UNDEF_ARGS(0);
    chiProcessorRequest(CHI_CURRENT_PROCESSOR, CHI_REQUEST_PROMOTE);
    AUX.YIELD = chiFromCont(&returnUnit);
    KNOWN_JUMP(chiYield); // Enter interrupt, execute garbage collector
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
    _PROLOGUE;
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    parkEnd(proc);
    chiEvent0(proc, THREAD_RUN_BEGIN);
    FIRST_JUMP(returnUnit);
}

CONT(chiProcessorPark) {
    PROLOGUE_INVARIANTS(chiProcessorPark);
    UNDEF_ARGS(0);
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
    UNDEF_ARGS(0);
    PROTECT_VOID(park(CHI_CURRENT_PROCESSOR));
}
#endif

CONT(chiProcessorInitLocal, .na = 1) {
    PROLOGUE_INVARIANTS(chiProcessorInitLocal);
    Chili local = AGET(0);
    UNDEF_ARGS(0);
    CHI_ASSERT(!chiTrue(AUX.PLOCAL));
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiPromoteLocal(proc, &local);
    AUX.PLOCAL = proc->local = local;
    ASET(0, CHI_FALSE);
    RET;
}

_Noreturn void chiProcessorEnter(ChiProcessor* proc) {
    CALLCONV_INIT;
    proc->reg.aux = &AUX;
    proc->reg.hl = &HLRW;
    CHI_IF(CHI_TRACEPOINTS_CONT_ENABLED, proc->worker->tp = &AUX.TRACEPOINT);
    chiHookProcRun(proc, START);
    AUX.PROC = chiFromPtr(proc);
    CHI_IF(CHI_FNLOG_ENABLED, AUX.FNLOG = chiEventEnabled(proc, FNLOG_CONT);)
    chiEvent0(proc, PROC_RUN_BEGIN);
    RESTORE_HEAP_REGS;
    ACTIVATE_THREAD;
    ASSERT_INVARIANTS;
    FIRST_JUMP(returnUnit);
}

#define OVERAPP_BODY(fn, rest)                  \
    CHI_ASSUME(rest <= CHI_AMAX);               \
    ASET(0, fn);                                \
    for (size_t i = 0; i < rest; ++i)           \
        ASET(i + 1, SP[i]);                     \
    APP(rest)

#define DEFINE_OVERAPP(rest)                                    \
    STATIC_CONT(overApp##rest, .na = 1, .size = rest + 1) {    \
        PROLOGUE_INVARIANTS(overApp##rest);                     \
        Chili fn = AGET(0);                                     \
        UNDEF_ARGS(0);                                          \
        SP -= rest + 1;                                         \
        OVERAPP_BODY(fn, rest);                                 \
    }

DEFINE_OVERAPP(1)
DEFINE_OVERAPP(2)
DEFINE_OVERAPP(3)
DEFINE_OVERAPP(4)

static const ChiCont overAppTable[] =
    { &overApp1, &overApp2, &overApp3, &overApp4 };

STATIC_CONT(overAppN, .na = 1, .size = CHI_VASIZE) {
    PROLOGUE_INVARIANTS(overAppN);
    Chili fn = AGET(0);
    UNDEF_ARGS(0);
    const size_t rest = (size_t)chiToUnboxed(SP[-2]) - 2;
    SP -= rest + 2;
    OVERAPP_BODY(fn, rest);
}

#define PARTIAL_BODY(args, delta)               \
    CHI_ASSERT(chiFnArity(clos) == delta);      \
    CHI_ASSERT(chiSize(clos) - 2 == args);      \
    CHI_ASSUME(delta <= CHI_AMAX);              \
    for (size_t i = delta; i > 0; --i)          \
        ASET(args + i, AGET(i));                \
    CHI_ASSUME(args <= CHI_AMAX - 1);           \
    for (size_t i = 0; i <= args; ++i)          \
        ASET(i, chiIdx(clos, i + 1));           \
    JUMP_FN(chiIdx(AGET(0), 0))

#define DEFINE_PARTIAL(args, arity)                             \
    STATIC_CONT(partial##args##of##arity, .na = arity - args + 1) {    \
        PROLOGUE_INVARIANTS(partial##args##of##arity);          \
        const size_t delta = arity - args;                      \
        UNDEF_ARGS(delta + 1);                                  \
        const Chili clos = AGET(0);                             \
        PARTIAL_BODY(args, delta);                              \
    }

DEFINE_PARTIAL(1, 2)
DEFINE_PARTIAL(1, 3)
DEFINE_PARTIAL(1, 4)
DEFINE_PARTIAL(1, 5)
DEFINE_PARTIAL(2, 3)
DEFINE_PARTIAL(2, 4)
DEFINE_PARTIAL(2, 5)
DEFINE_PARTIAL(3, 4)
DEFINE_PARTIAL(3, 5)
DEFINE_PARTIAL(4, 5)

STATIC_CONT(partialNofM, .na = CHI_VAARGS) {
    PROLOGUE_INVARIANTS(partialNofM);
    const Chili clos = AGET(0);
    const size_t args = chiSize(clos) - 2, delta = chiFnArity(clos);
    PARTIAL_BODY(args, delta);
}

#define CHI_PARTIAL_TABLE(args, arity, ...)                             \
    static const ChiCont partialTable##args[CHI_AMAX - args - 1] =      \
        { __VA_ARGS__, [arity - args ... CHI_AMAX - args - 2] = &partialNofM };
CHI_PARTIAL_TABLE(1, 5, &partial1of2, &partial1of3, &partial1of4, &partial1of5)
CHI_PARTIAL_TABLE(2, 5, &partial2of3, &partial2of4, &partial2of5)
CHI_PARTIAL_TABLE(3, 5, &partial3of4, &partial3of5)
CHI_PARTIAL_TABLE(4, 5, &partial4of5)
#undef CHI_PARTIAL_TABLE

#define APP_BODY(args, arity, partial)                                  \
    CHI_ASSERT(arity < CHI_AMAX);                                       \
    if (args < arity) {                                                 \
        /*LIMITS_PROC(.heap=args + 2);*/                                \
        LIMITS_PROC(.heap=CHI_AMAX + 2);                                \
        NEW(clos, CHI_FN(arity - args), args + 2);                      \
        NEW_INIT(clos, 0, chiFromCont(partial));                        \
        for (size_t i = 0; i <= args; ++i)                              \
            NEW_INIT(clos, i + 1, AGET(i));                             \
        UNDEF_ARGS(0);                                                  \
        ASET(0, clos);                                                  \
        RET;                                                            \
    }                                                                   \
    if ((__builtin_constant_p(args) ? args : 2) > 1 && args > arity) {  \
        /*LIMITS_PROC(.stack=rest + 2);*/                               \
        LIMITS_PROC(.stack=CHI_AMAX + 2);                               \
        const size_t rest = args - arity;                               \
        for (size_t i = 0; i < rest; ++i) {                             \
            SP[i] = AGET(1 + arity + i);                                \
            AUNDEF(1 + arity + i);                                      \
        }                                                               \
        SP += rest;                                                     \
        if ((__builtin_constant_p(args) ? args - 1 : rest) - 1 < CHI_DIM(overAppTable)) { \
            *SP++ = chiFromCont(overAppTable[rest - 1]);                \
        } else {                                                        \
            *SP++ = _chiFromUnboxed(rest + 2);                          \
            *SP++ = chiFromCont(&overAppN);                             \
        }                                                               \
    }                                                                   \
    JUMP_FN(chiIdx(AGET(0), 0))

#define DEFINE_APP(args)                                \
    CONT(chiApp##args, .na = args + 1) {               \
        PROLOGUE_INVARIANTS(chiApp##args);              \
        UNDEF_ARGS(args + 1);                           \
        const size_t arity = chiFnArity(AGET(0));       \
        APP_BODY(args, arity,                           \
                 partialTable##args[arity - args - 1]); \
    }

DEFINE_APP(1)
DEFINE_APP(2)
DEFINE_APP(3)
DEFINE_APP(4)

CONT(chiAppN, .na = CHI_VAARGS) {
    PROLOGUE_INVARIANTS(chiAppN);
    const size_t args = AUX.VAARGS - 1U, arity = chiFnArity(AGET(0));
    CHI_ASSERT(args < CHI_AMAX);
    APP_BODY(args, arity, &partialNofM);
}
