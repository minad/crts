#pragma once

#include "gc.h"
#include "processor.h"
#include "stats.h"

/**
 * Runtime hooks
 * Hooks with even id are executed in reverse order.
 */
typedef enum {
    CHI_HOOK_PROC_START      = 0,
    CHI_HOOK_PROC_STOP       = 1, // reverse
    CHI_HOOK_WORKER_START    = 2,
    CHI_HOOK_WORKER_STOP     = 3, // reverse
    CHI_HOOK_COUNT           = 4,
} ChiHookType;

#define ChiHookFn(t)                typeof ((void (*)(ChiHookType, t*))0)
#define chiHookProc(rt, t, fn)      ({ ChiHookFn(ChiProcessor) f = (fn); _chiHook((rt), CHI_HOOK_PROC_##t, (ChiHookFn(void))f); })
#define chiHookWorker(rt, t, fn)    ({ ChiHookFn(ChiWorker) f = (fn); _chiHook((rt), CHI_HOOK_WORKER_##t, (ChiHookFn(void))f); })
#define chiHookProcRun(proc, t)     ({ ChiProcessor* p = (proc); _chiHookRun(p->rt, CHI_HOOK_PROC_##t, p); })
#define chiHookWorkerRun(worker, t) ({ ChiWorker* w = (worker); _chiHookRun(w->rt, CHI_HOOK_WORKER_##t, w); })

typedef struct ChiEventState_ ChiEventState;
typedef struct ChiProfiler_ ChiProfiler;
typedef struct ChiModuleEntry_ ChiModuleEntry;

/**
 * Hash table used to store loaded modules
 */
typedef struct {
    size_t used, capacity;
    ChiModuleEntry* entry;
} ChiModuleHash;

typedef struct {
    ChiHookFn(void) fn;
    ChiHookType type;
} ChiHook;

typedef struct {
    size_t used, capacity;
    ChiHook* elem;
} ChiHookVec;

typedef struct {
    ChiSigHandler sigHandler;
    ChiTimer      timer;
} ChiInterrupts;

typedef void (*ChiExitHandler)(int);

/**
 * Main runtime object. The whole global
 * runtime state is collected here.
 *
 * It is possible to have multiple parallel
 * runtimes in a process, which don't share the heap.
 * However the runtimes share the chunk memory
 * manager and the signal handlers.
 */
typedef struct ChiRuntime_ {
    struct {
        ChiNanos         start, end;
    } timeRef;
    struct {
        ChiMutex         mutex;
        ChiCond          cond;
        CHI_CHOICE(CHI_SYSTEM_HAS_TASK, ChiProcessorList list, ChiProcessor single);
    } proc;
    CHI_IF(CHI_SYSTEM_HAS_TASK, _Atomic(uint32_t) nextWid;)
    CHI_IF(CHI_SYSTEM_HAS_INTERRUPT, ChiInterrupts interrupts;)
    CHI_IF(CHI_EVENT_ENABLED, ChiEventState* eventState;)
    CHI_IF(CHI_PROF_ENABLED, ChiProfiler* profiler;)
    CHI_IF(CHI_STATS_ENABLED, ChiStats stats;)
    ChiHookVec           hooks[CHI_HOOK_COUNT];
    ChiGC                gc;
    Chili                entryPoints, fail;
    ChiHeap              heap;
    _Atomic(size_t)      heapTotalSize;
    size_t               heapOverflowLimit;
    ChiModuleHash        moduleHash;
    ChiRuntimeOption     option;
    char**               argv;
    uint32_t             argc;
    int32_t              exitCode;
    ChiExitHandler       exitHandler;
    ChiTrigger           heapOverflow;
} ChiRuntime;

// AsyncException tags. Must be ordered alphabetically.
typedef enum {
    CHI_HEAP_OVERFLOW,
    CHI_STACK_OVERFLOW,
    CHI_THREAD_INTERRUPT,
    CHI_USER_INTERRUPT,
} ChiAsyncException;

CHI_INTERN_CONT_DECL(chiYield)
CHI_INTERN_CONT_DECL(chiJumpCont)
CHI_INTERN_CONT_DECL(chiRestoreCont)
CHI_INTERN_CONT_DECL(chiRunMainCont)
CHI_INTERN_CONT_DECL(chiSetupEntryPointsCont)
CHI_INTERN_CONT_DECL(chiStartupEndCont)
CHI_INTERN_CONT_DECL(chiThunkForward)
CHI_INTERN_CONT_DECL(chiThrowRuntimeException)
CHI_INTERN_CONT_DECL(chiExceptionCatchCont)

CHI_INTERN void _chiHook(ChiRuntime*, ChiHookType, ChiHookFn(void));
CHI_INTERN void _chiHookRun(ChiRuntime*, ChiHookType, void*);
CHI_INTERN _Noreturn void chiRuntimeMain(ChiRuntime*, int, char**, ChiExitHandler);
CHI_INTERN _Noreturn void chiRuntimeEnter(ChiRuntime*, int, char**);
CHI_INTERN void chiRuntimeInit(ChiRuntime*, ChiExitHandler);
CHI_INTERN void chiRuntimeDestroy(ChiProcessor*);
