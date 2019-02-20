#pragma once

#include "processor.h"
#include "heap.h"
#include "rtoption.h"
#include "stats.h"

/**
 * Entry points into the Chili runtime.
 * The entry points are Chili closures.
 */
typedef struct {
    Chili exit;
    Chili unhandled;
    Chili par;
    Chili scheduler;
    Chili interrupt;
} ChiEntryPoint;

/**
 * Runtime hooks
 */
typedef enum {
    CHI_HOOK_RUNTIME_DESTROY,
    CHI_HOOK_PROC_START,
    CHI_HOOK_PROC_STOP,
    CHI_HOOK_WORKER_START,
    CHI_HOOK_WORKER_STOP,
    _CHI_HOOK_COUNT,
} ChiHookType;

typedef void (*ChiHookFn)(ChiWorker*, ChiHookType);
typedef struct ChiLocInfo_ ChiLocInfo;
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
    ChiSigHandler     sigHandler[_CHI_SIG_MAX + 1];
    ChiTimer          timer;
    _Atomic(uint32_t) userInterrupt;
} ChiInterruptSupport;

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
    int32_t              exitCode;
    _Atomic(bool)        heapOverflow;
    _Atomic(size_t)      heapSize;
    struct {
        ChiTime          start, end;
    } timeRef;
    ChiInterruptSupport  interruptSupport[CHI_SYSTEM_HAS_INTERRUPT];
    ChiHookVec           hooks[_CHI_HOOK_COUNT];
    _Atomic(uint32_t)    nextTid, nextWid, threadCount;
    ChiGC                gc;
    ChiEventState*       eventState;
    ChiProfiler*         profiler;
    ChiStats             stats;
    ChiEntryPoint        entryPoint;
    Chili                blackhole, switchThreadClos, enterContClos;
    ChiProcessorManager  procMan;
    ChiHeap              heap;
    ChiTenure            tenure;
    ChiBlockManager      blockTemp;
    ChiModuleHash        moduleHash;
    char**               argv;
    uint32_t             argc;
    ChiRuntimeOption     option;
} ChiRuntime;

CHI_CONT_DECL(extern, chiStartupEndCont)
CHI_CONT_DECL(extern, chiSetupEntryPointsCont)
CHI_CONT_DECL(extern, chiRunMainCont)
CHI_CONT_DECL(extern, chiBlackhole)
CHI_CONT_DECL(extern, chiShowAsyncException)
CHI_CONT_DECL(extern, chiIdentity)
CHI_CONT_DECL(extern, chiSwitchThread)
CHI_CONT_DECL(extern, chiEnterCont)
CHI_CONT_DECL(extern, chiThunkThrow)
CHI_CONT_DECL(extern, chiEntryPointExitDefault)
CHI_CONT_DECL(extern, chiEntryPointUnhandledDefault)
CHI_CONT_DECL(extern, chiJumpCont)
CHI_CONT_DECL(extern, chiExitDefault)

void chiHook(ChiRuntime*, ChiHookType, ChiHookFn);
void chiHookRun(ChiWorker*, ChiHookType);
void chiInit(ChiRuntime*);
void chiDestroy(ChiRuntime*);
_Noreturn void chiRun(ChiRuntime*, int, char**);
_Noreturn void chiEnter(void);
_Noreturn void chiWasmContinue(void);
_Noreturn void chiRuntimeMain(ChiRuntime*, int, char**);
