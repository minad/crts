#pragma once

#include "minorheap.h"
#include "heap.h"
#include "gc.h"

#if CHI_SYSTEM_HAS_TASKS
#  define CHI_FOREACH_PROCESSOR(proc, rt) \
    for (ChiProcessor* proc = (rt)->procMan.list.head; proc; proc = proc->next)
#  define CHI_FOREACH_OTHER_PROCESSOR_HEAD(proc, currProc, head)        \
    for (ChiProcessor *_curr = (currProc),                              \
             *_next = (head),                                           \
             *proc = (_next == _curr ? _curr->next : _next);            \
         proc; _next = proc->next, proc = (_next == _curr ? _curr->next : _next))
#else
#  define CHI_FOREACH_PROCESSOR(proc, rt) \
    for (ChiProcessor* proc = (rt)->procMan.list.head; proc; proc = 0)
#  define CHI_FOREACH_OTHER_PROCESSOR_HEAD(proc, currProc, head)        \
    for (ChiProcessor* _curr = (currProc), *proc = (head); 0; CHI_NOWARN_UNUSED(_curr))
#endif

#define CHI_FOREACH_OTHER_PROCESSOR(proc, currProc)                     \
    CHI_FOREACH_OTHER_PROCESSOR_HEAD(proc, currProc, _curr->rt->procMan.list.head)

typedef struct ChiRuntime_    ChiRuntime;
typedef struct ChiEventWS_    ChiEventWS;
typedef struct ChiProfilerWS_ ChiProfilerWS;

/**
 * Each native ChiTask associated with a ChiRuntime
 * is a worker. The current worker can be accessed via CHI_CURRENT_WORKER.
 */
typedef struct ChiWorker_ {
    ChiRuntime*       rt;
    ChiEventWS*       eventWS;
    ChiProfilerWS*    profilerWS;
    volatile int32_t* tp;
    volatile int32_t  _tp;
    uint32_t          wid;
    char              name[16];
} ChiWorker;

/**
 * Processor states
 *
 * @image html processor-statemachine.png
 */
typedef enum {
    CHI_PROCESSOR_RUN,
    CHI_PROCESSOR_FINAL,
    CHI_PROCESSOR_PROTECT,
    CHI_PROCESSOR_SYNC,
    CHI_PROCESSOR_STOP,
} ChiProcessorState;

typedef enum {
    CHI_PM_INITIAL,
    CHI_PM_FINAL,
    CHI_PM_RUN,
    CHI_PM_SYNC,
    CHI_PM_UNSYNC,
} ChiPMState;

/**
 * Processors are special ChiWorker%s, which execute and schedule
 * ChiThread%s.
 */
typedef struct ChiProcessor_ {
    ChiWorker         worker;
    struct {
        ChiWord**     hl;
        bool          interrupt;
        bool          dumpStack;
        bool          tick;
        ChiNanos      nextTick;
    } trigger;
    struct {
        ChiMutex     mutex;
        ChiCond      cond;
        ChiProcessorState id;
    } state;
    struct {
        ChiMutex    mutex;
        ChiCond     cond;
        ChiMillis   timeout;
    } suspend;
    ChiNursery      nursery;
    ChiHeapHandle   heapHandle;
    Chili           schedulerThread, thread;
    ChiGCPS         gcPS;
    void*           interpPS; ///< Used by interpreter
    ChiRuntime*     rt;
    ChiTask         task;
    _Atomic(ChiProcessor*) next;
    ChiProcessor*   nextStopped;
} ChiProcessor;

/**
 * Datastructure holding everything needed
 * for processor management.
 */
typedef struct {
    _Atomic(ChiPMState)       state;
    _Atomic(uint32_t)         running;
    /*
     * RCU singly linked list of processors.
     * Readers must increment/decrement critical
     * around a critical sections.
     * Processors are moved first to the stopped
     * list and destroyed for critical == 0.
     */
    struct {
        _Atomic(uint32_t)      critical;
        _Atomic(ChiProcessor*) head;
        ChiProcessor*          stopped;
    } list;
} ChiProcessorManager;

extern _Thread_local ChiProcessor _chiProcessor;

// Avoid accessing these global variables as much as possible!
// In principle they should be used only in functions,
// which cannot get the processor/worker as argument (e.g., external api functions).
#define CHI_CURRENT_PROCESSOR ({ CHI_ASSERT(_chiProcessor.rt); &_chiProcessor; })
#define CHI_CURRENT_WORKER    ({ CHI_ASSERT(_chiProcessor.worker.rt); &_chiProcessor.worker; })
#define CHI_CURRENT_RUNTIME   (CHI_CURRENT_WORKER->rt)

void chiProcessorSetup(ChiProcessor*);
void chiProcessorSetupDone(ChiProcessor*);
void chiProcessorProtectEnd(ChiProcessor*);
bool chiProcessorSync(ChiProcessor*);
_Noreturn void chiProcessorStopped(ChiProcessor*);
uint32_t chiProcessorTryStart(ChiRuntime*);
void chiProcessorInterrupt(ChiRuntime*);
void chiProcessorSignal(ChiRuntime*, ChiSig);
void chiProcessorTick(ChiRuntime*);
ChiProcessor* chiProcessorBegin(ChiRuntime*);
ChiWorker* chiWorkerBegin(ChiRuntime*, const char*);
void chiWorkerEnd(ChiWorker*);

CHI_INL ChiProcessor* chiWorkerToProcessor(ChiWorker* worker) {
    ChiProcessor* proc = (ChiProcessor*)(void*)worker;
    CHI_ASSERT(proc->rt == worker->rt);
    return proc;
}

CHI_INL CHI_WU bool chiMainWorker(ChiWorker* worker) {
    return !worker->wid;
}
