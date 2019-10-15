#pragma once

#include "localgc.h"
#include "localheap.h"

#if CHI_SYSTEM_HAS_TASK
#  define CHI_FOREACH_PROCESSOR(p, rt)          CHI_LIST_FOREACH_NODELETE(ChiProcessor, link, p, &(rt)->proc.list)
/* Avoid accessing these global variables as much as possible!
 * In principle they should be used only in functions,
 * which cannot get the processor/worker as argument (e.g., external api functions).
 */
#  define CHI_CURRENT_PROCESSOR                 ({ CHI_ASSERT(_chiProcessor.rt); &_chiProcessor; })
#  define CHI_CURRENT_RUNTIME                   (CHI_CURRENT_PROCESSOR->rt)
#else
#  define CHI_FOREACH_PROCESSOR(p, rt)          for (ChiProcessor* p = &(rt)->proc.single; p; p = 0)
#  define CHI_CURRENT_PROCESSOR                 (&CHI_CURRENT_RUNTIME->proc.single)
#  define CHI_CURRENT_RUNTIME                   ({ CHI_ASSERT(_chiRuntime); _chiRuntime; })
#endif

typedef struct ChiRuntime_    ChiRuntime;
typedef struct ChiEventWS_    ChiEventWS;
typedef struct ChiProfilerWS_ ChiProfilerWS;

/**
 * Each native ChiTask associated with a ChiRuntime is a worker.
 */
typedef struct ChiWorker_ {
    ChiRuntime* rt;
    uint32_t    wid;
    char        name[12];
    CHI_IF(CHI_TRACEPOINTS_ENABLED, _Atomic(int32_t)* tp, tpDummy;)
    CHI_IF(CHI_EVENT_ENABLED, ChiEventWS* eventWS;)
    CHI_IF(CHI_PROF_ENABLED, ChiProfilerWS* profilerWS;)
} ChiWorker;

#if CHI_SYSTEM_HAS_TASK
typedef enum {
    CHI_SECONDARY_INACTIVE,
    CHI_SECONDARY_ACTIVE,
    CHI_SECONDARY_RUN,
    CHI_SECONDARY_STOP,
} ChiSecondaryStatus;

typedef struct {
    ChiWorker          worker;
    ChiTask            task;
    ChiMutex           mutex;
    ChiCond            cond;
    ChiSecondaryStatus status;
} ChiSecondary;

typedef struct ChiProcessorLink_ ChiProcessorLink;
struct ChiProcessorLink_ { ChiProcessorLink *prev, *next; };
#endif

/**
 * Processors execute and schedule ChiThread%s.
 * Each processor has a primary and secondary worker.
 */
typedef struct ChiProcessor_ {
#if CHI_SYSTEM_HAS_TASK
    struct {
        ChiWorker     worker;
        ChiTask       task;
    } primary;
    ChiSecondary      secondary;
    ChiWorker*        worker;
    ChiProcessorLink  link;
#else
    union {
        struct { ChiWorker worker; } primary;
        ChiWorker worker[1];
    };
#endif
    ChiLocalHeap      heap;
    ChiLocalGC        gc;
    ChiHeapHandle     handle;
    Chili             local, thread; // Local roots
    ChiRuntime*       rt;
    CHI_IF(CHI_CBY_SUPPORT_ENABLED, struct CbyInterpPS_* interpPS;)
    struct {
        ChiWord**     hl;
        ChiWord*      hlDummy;
        ChiAuxRegs*   aux;
    } reg;
    struct {
        ChiMutex      mutex;
        ChiCond       cond;
        ChiMillis     timeout;
    } suspend;
    struct {
        CHI_UNLESS(CHI_SYSTEM_HAS_INTERRUPT, ChiNanos lastTick;)
        ChiTrigger    interrupt;
        ChiTrigger    exit;
        ChiTrigger    tick;
        ChiTrigger    dumpStack;
        ChiTrigger    userInterrupt;
        ChiTrigger    scavenge;
        ChiTrigger    migrate;
    } trigger;
} ChiProcessor;

#if CHI_SYSTEM_HAS_TASK
#  define LIST_PREFIX chiProcessorList
#  define LIST_ELEM   ChiProcessor
#  include "generic/list.h"
extern _Thread_local ChiProcessor _chiProcessor;
#else
extern ChiRuntime* _chiRuntime;
#endif

CHI_INTERN void chiProcessorSetup(ChiProcessor*);
CHI_INTERN void chiProcessorProtectEnd(ChiProcessor*);
CHI_INTERN void chiProcessorService(ChiProcessor*);
CHI_INTERN _Noreturn void chiProcessorEnter(ChiProcessor*);
CHI_INTERN ChiProcessor* chiProcessorBegin(ChiRuntime*);
CHI_INTERN void chiWorkerBegin(ChiWorker*, ChiRuntime*, const char*);
CHI_INTERN void chiWorkerEnd(ChiWorker*);
CHI_INTERN void chiProcessorRequestAll(ChiRuntime*, ChiProcessorRequest);
CHI_INTERN void chiProcessorRequestMain(ChiRuntime*, ChiProcessorRequest);
CHI_INTERN void chiProcessorRequest(ChiProcessor*, ChiProcessorRequest);

CHI_INL CHI_WU bool chiWorkerMain(ChiWorker* worker) {
    return !CHI_SYSTEM_HAS_TASK || !worker->wid;
}

CHI_INL CHI_WU bool chiProcessorMain(ChiProcessor* proc) {
    return chiWorkerMain(&proc->primary.worker);
}
