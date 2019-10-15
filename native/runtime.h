#pragma once

#include "gc.h"
#include "processor.h"
#include "stats.h"

/**
 * Entry points into the Chili runtime.
 * The entry points are Chili closures.
 */
typedef struct {
    Chili start;
    Chili unhandled;
    Chili blackhole;
    Chili timerInterrupt;
    Chili userInterrupt;
} ChiEntryPoint;

/**
 * Runtime hooks
 * Hooks with even id are executed in reverse order.
 */
typedef enum {
    CHI_HOOK_PROC_START      = 0,
    CHI_HOOK_PROC_STOP       = 1, // reverse
    CHI_HOOK_PROC            = CHI_HOOK_PROC_STOP,
    CHI_HOOK_WORKER_START    = 2,
    CHI_HOOK_WORKER_STOP     = 3, // reverse
    CHI_HOOK_COUNT           = 4,
} ChiHookType;

typedef void (*ChiHookFn)(ChiHookType, void*);
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
    ChiHookFn   fn;
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
        ChiTime          start, end;
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
    ChiEntryPoint        entryPoint;
    ChiHeap              heap;
    _Atomic(size_t)      totalHeapChunkSize;
    ChiModuleHash        moduleHash;
    ChiRuntimeOption     option;
    char**               argv;
    uint32_t             argc;
    int32_t              exitCode;
    ChiExitHandler       exitHandler;
    ChiTrigger           heapOverflow;
} ChiRuntime;

CHI_INTERN_CONT_DECL(chiEntryPointUnhandledDefault)
CHI_INTERN_CONT_DECL(chiIdentity)
CHI_INTERN_CONT_DECL(chiJumpCont)
CHI_INTERN_CONT_DECL(chiRestoreCont)
CHI_INTERN_CONT_DECL(chiRunMainCont)
CHI_INTERN_CONT_DECL(chiSetupEntryPointsCont)
CHI_INTERN_CONT_DECL(chiShowAsyncException)
CHI_INTERN_CONT_DECL(chiStartupEndCont)
CHI_INTERN_CONT_DECL(chiThunkBlackhole)
CHI_INTERN_CONT_DECL(chiThunkForward)

CHI_INTERN void chiHook(ChiRuntime*, ChiHookType, ChiHookFn);
CHI_INTERN void chiHookRun(ChiHookType, void*);
CHI_INTERN _Noreturn void chiRuntimeMain(ChiRuntime*, int, char**, ChiExitHandler);
CHI_INTERN _Noreturn void chiRuntimeEnter(ChiRuntime*, int, char**);
CHI_INTERN void chiRuntimeInit(ChiRuntime*, ChiExitHandler);
CHI_INTERN void chiRuntimeDestroy(ChiProcessor*);
