#include "barrier.h"
#include "error.h"
#include "event.h"
#include "new.h"
#include "sink.h"
#include "stack.h"
#include "tracepoint.h"

CHI_CHOICE(CHI_SYSTEM_HAS_TASK, _Thread_local ChiProcessor _chiProcessor, ChiRuntime* _chiRuntime = 0);

/*
void AnnotateIgnoreWritesBegin(const char*, int);
void AnnotateIgnoreWritesEnd(const char*, int);
#define CHI_ANNOTATE_IGNORE_WRITES_BEGIN AnnotateIgnoreWritesBegin(__FILE__, __LINE__)
#define CHI_ANNOTATE_IGNORE_WRITES_END AnnotateIgnoreWritesEnd(__FILE__, __LINE__)
*/
#define CHI_ANNOTATE_IGNORE_WRITES_BEGIN ({})
#define CHI_ANNOTATE_IGNORE_WRITES_END   ({})

static void interruptProcessor(ChiProcessor* proc) {
    chiTrigger(&proc->trigger.interrupt, true);
    CHI_ANNOTATE_IGNORE_WRITES_BEGIN;
    *proc->reg.hl = 0;
    CHI_ANNOTATE_IGNORE_WRITES_END;
}

void chiProcessorRequest(ChiProcessor* proc, ChiProcessorRequest request) {
    chiEvent(proc->rt, PROC_REQUEST, .request = request);

#if CHI_SYSTEM_HAS_TASK
    ChiSecondary* snd = &proc->secondary;
    CHI_LOCK_MUTEX(&snd->mutex);
#endif

    switch (request) {
    case CHI_REQUEST_TICK:
        if (CHI_AND(CHI_SYSTEM_HAS_TASK, snd->status != CHI_SECONDARY_INACTIVE))
            return; // No ticks if secondary worker is active/running
        if (chiTriggered(&proc->trigger.tick))
            chiEvent(proc->rt, PROC_STALL, .wid = proc->primary.worker.wid);
        else
            chiTrigger(&proc->trigger.tick, true);
        break;
    case CHI_REQUEST_EXIT: chiTrigger(&proc->trigger.exit, true); break;
    case CHI_REQUEST_DUMPSTACK: chiTrigger(&proc->trigger.dumpStack, true); break;
    case CHI_REQUEST_USERINTERRUPT: chiTrigger(&proc->trigger.userInterrupt, true); break;
    case CHI_REQUEST_MIGRATE: chiTrigger(&proc->trigger.migrate, true); // fall through
    case CHI_REQUEST_SCAVENGE: chiTrigger(&proc->trigger.scavenge, true); break;
    case CHI_REQUEST_HANDSHAKE: break;
    }

    interruptProcessor(proc);

    if (request == CHI_REQUEST_USERINTERRUPT || request == CHI_REQUEST_EXIT) {
        CHI_LOCK_MUTEX(&proc->suspend.mutex);
        if (!chiMillisZero(proc->suspend.timeout)) {
            proc->suspend.timeout = chiMillis(0);
            chiCondSignal(&proc->suspend.cond);
        }
        /* TODO chiTaskCancel interfers with CHI_LOCK_MUTEX(&snd->mutex) in chiSecondaryDeactivate.
           How can I ensure that chiTaskCancel gets really only submitted to the protected function call?
           Probably by activating/deactivating signals around the protected function call
           (chiTaskCancelEnable/chiTaskCancelDisable).
#if CHI_SYSTEM_HAS_TASK
        else if (snd->status != CHI_SECONDARY_INACTIVE) {
            chiTaskCancel(proc->primary.task);
        }
#endif
        */
    }

#if CHI_SYSTEM_HAS_TASK
    if (snd->status == CHI_SECONDARY_ACTIVE)
        chiCondSignal(&snd->cond);
#endif
}

static void workerBegin(ChiWorker* worker, ChiRuntime* rt, const char* prefix, uint32_t wid) {
    worker->rt = rt;
    worker->wid = wid;
    CHI_IF(CHI_TRACEPOINTS_ENABLED, worker->tp = &worker->tpDummy);
    chiFmt(worker->name, sizeof (worker->name), "%s%u", prefix, wid);
    CHI_IF(CHI_SYSTEM_HAS_TASK, chiTaskName(worker->name));
    chiEventWorkerStart(worker);
    chiHookRun(CHI_HOOK_WORKER_START, worker);
}

#if CHI_POISON_ENABLED
CHI_EXPORT _Thread_local struct _ChiDebugData _chiDebugData;
static void resetDebugData(void) {
    CHI_POISON_STRUCT(&_chiDebugData, CHI_POISON_DESTROYED);
}
static void setupDebugData(ChiProcessor* proc) {
    _chiDebugData.ownerMinor = (uintptr_t)&proc->heap.manager;
    _chiDebugData.ownerMinorMask = CHI_ALIGNMASK(proc->rt->option.block.size);
    _chiDebugData.wid = proc->primary.worker.wid;
    _chiDebugData.markState = proc->gc.markState;
    _chiDebugData.protect = false;
}
#else
static void resetDebugData(void) {}
static void setupDebugData(ChiProcessor* CHI_UNUSED(proc)) {}
#endif

#if CHI_SYSTEM_HAS_TASK
static bool initProcessor(ChiProcessor*);
static void exitProcessor(ChiProcessor*);

CHI_TASK(secondaryRun, arg) {
    ChiProcessor* proc = (ChiProcessor*)arg;
    ChiSecondary* snd = &proc->secondary;
    workerBegin(&snd->worker, proc->rt, "back", proc->primary.worker.wid + 1);
    for (;;) {
        {
            CHI_LOCK_MUTEX(&snd->mutex);
            if (snd->status == CHI_SECONDARY_STOP)
                break;
            if (snd->status == CHI_SECONDARY_INACTIVE ||
                (snd->status == CHI_SECONDARY_ACTIVE
                 && !chiTriggered(&proc->trigger.interrupt))) {
                chiCondWait(&snd->cond, &snd->mutex);
                continue;
            }
            snd->status = CHI_SECONDARY_RUN;
        }

        proc->worker = &snd->worker;
        CHI_TTP(proc, .name = "runtime;suspend");
        setupDebugData(proc);
        chiProcessorService(proc);
        resetDebugData();
        proc->worker = &proc->primary.worker;

        {
            CHI_LOCK_MUTEX(&snd->mutex);
            if (!chiTriggered(&proc->trigger.interrupt)) {
                snd->status = CHI_SECONDARY_ACTIVE;
                chiCondSignal(&snd->cond);
            }
        }
    }
    chiWorkerEnd(&snd->worker);
    return 0;
}

static bool secondarySetup(ChiSecondary* snd, ChiProcessor* proc) {
    chiMutexInit(&snd->mutex);
    chiCondInit(&snd->cond);
    snd->task = chiTaskTryCreate(secondaryRun, proc);
    if (chiTaskNull(snd->task)) {
        chiMutexDestroy(&snd->mutex);
        chiCondDestroy(&snd->cond);
        return false;
    }
    return true;
}

static void secondaryStop(ChiSecondary* snd) {
    {
        CHI_LOCK_MUTEX(&snd->mutex);
        CHI_ASSERT(snd->status == CHI_SECONDARY_INACTIVE);
        snd->status = CHI_SECONDARY_STOP;
        chiCondSignal(&snd->cond);
    }
    chiTaskJoin(snd->task);
}

static void secondaryActivate(ChiProcessor* proc) {
    ChiSecondary* snd = &proc->secondary;
    CHI_LOCK_MUTEX(&snd->mutex);
    CHI_ASSERT(snd->status == CHI_SECONDARY_INACTIVE);
    snd->status = CHI_SECONDARY_ACTIVE;
    if (chiTriggered(&proc->trigger.interrupt))
        chiCondSignal(&snd->cond);
    CHI_IF(CHI_POISON_ENABLED, _chiDebugData.protect = true);
}

static void secondaryDeactivate(ChiProcessor* proc) {
    ChiSecondary* snd = &proc->secondary;
    CHI_LOCK_MUTEX(&snd->mutex);
    while (snd->status != CHI_SECONDARY_ACTIVE) {
        CHI_ASSERT(snd->status == CHI_SECONDARY_RUN);
        chiCondWait(&snd->cond, &snd->mutex);
    }
    snd->status = CHI_SECONDARY_INACTIVE;
    CHI_IF(CHI_POISON_ENABLED, _chiDebugData.protect = false);
    setupDebugData(proc);
}

typedef struct {
    ChiMutex          mutex;
    ChiCond           cond;
    ChiRuntime*       rt;
    Chili             thread;
    ChiGCPhase        phase;
    ChiMarkState      markState;
    ChiColor          allocColor;
    int32_t           result;
} ProcessorArg;

CHI_INL void inheritGCState(ChiProcessor* proc, const ProcessorArg* arg) {
    ChiLocalGC* gc = &proc->gc;
    atomic_store_explicit(&gc->phase, arg->phase, memory_order_relaxed);
    gc->markState = arg->markState;
    CHI_IF(CHI_POISON_ENABLED, _chiDebugData.markState = gc->markState;)
    proc->handle.allocColor = arg->allocColor;
}

CHI_TASK(processorRun, _arg) {
    ProcessorArg* arg = (ProcessorArg*)_arg;
    ChiProcessor* proc = chiProcessorBegin(arg->rt);
    proc->thread = arg->thread;

    bool ok = initProcessor(proc);
    inheritGCState(proc, arg);

    {
        CHI_LOCK_MUTEX(&arg->mutex);
        arg->result = ok ? 1 : -1;
        chiCondSignal(&arg->cond);
    }

    if (ok)
        chiProcessorEnter(proc);

    return 0;
}

uint32_t chiProcessorCount(void) {
    return CHI_CURRENT_RUNTIME->option.processors;
}

bool chiProcessorTryStart(Chili thread) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiLocalGC* gc = &proc->gc;
    ProcessorArg arg = { .thread = thread, .rt = proc->rt, .result = 0,
                         .phase = atomic_load_explicit(&gc->phase, memory_order_relaxed),
                         .markState = gc->markState,
                         .allocColor = proc->handle.allocColor };
    chiMutexInit(&arg.mutex);
    chiCondInit(&arg.cond);
    ChiTask t = chiTaskTryCreate(processorRun, &arg);
    if (chiTaskNull(t)) {
        arg.result = -1;
    } else {
        chiTaskClose(t);
        CHI_LOCK_MUTEX(&arg.mutex);
        while (!arg.result)
            chiCondWait(&arg.cond, &arg.mutex);
    }
    chiMutexDestroy(&arg.mutex);
    chiCondDestroy(&arg.cond);
    return arg.result > 0;
}

void chiProcessorStop(void) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(!chiProcessorMain(proc));
    chiEvent0(proc, THREAD_RUN_END);
    chiStackDeactivate(proc, 0);
    exitProcessor(proc);
}

static void waitProcessors(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    CHI_LOCK_MUTEX(&rt->proc.mutex);
    while (rt->proc.list.head.next != &proc->link || rt->proc.list.head.prev != &proc->link) {
        CHI_FOREACH_PROCESSOR (p, rt)
            chiProcessorRequest(p, CHI_REQUEST_EXIT);
        chiCondWait(&rt->proc.cond, &rt->proc.mutex);
    }
}
#endif

static bool initProcessor(ChiProcessor* proc) {
#if CHI_SYSTEM_HAS_TASK
    if (!secondarySetup(&proc->secondary, proc))
        return false;
    proc->primary.task = chiTaskCurrent();
#endif

    proc->reg.hl = &proc->reg.hlDummy;
    chiMutexInit(&proc->suspend.mutex);
    chiCondInit(&proc->suspend.cond);

    ChiRuntime* rt = proc->rt;
    const ChiRuntimeOption* opt = &rt->option;
    chiLocalGCSetup(&proc->gc, &rt->gc);
    setupDebugData(proc);
    chiLocalHeapSetup(&proc->heap,
                      opt->block.size, opt->block.chunk,
                      opt->block.nursery, rt);
    chiHeapHandleSetup(&proc->handle, &rt->heap, proc->gc.markState.white);

#if CHI_SYSTEM_HAS_TASK
    CHI_LOCK_MUTEX(&rt->proc.mutex);
    chiProcessorListPoison(proc);
    chiProcessorListAppend(&rt->proc.list, proc);
#endif

    return true;
}

static void destroyProcessor(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;

    CHI_IF(CHI_SYSTEM_HAS_TASK, secondaryStop(&proc->secondary);)

    // Destroy GC before hooks, in order to stop gc workers
    chiLocalGCDestroy(&proc->gc, &rt->gc);
    if (chiProcessorMain(proc))
        chiGCDestroy(rt);

    chiEvent0(proc, PROC_RUN_END);
    chiHookRun(CHI_HOOK_PROC_STOP, proc);
    chiEvent0(proc, PROC_DESTROY);

    chiHeapHandleDestroy(&proc->handle);
    chiLocalHeapDestroy(&proc->heap);
    resetDebugData();

    // This will also merge stats and write final events
    chiWorkerEnd(proc->worker);

#if CHI_SYSTEM_HAS_TASK
    // Remove from processor list
    {
        CHI_LOCK_MUTEX(&rt->proc.mutex);
        chiProcessorListDelete(proc);
        chiCondSignal(&rt->proc.cond);
    }

    // TODO XXXX There is a race condition here since the processor could still be accessed even
    // if removed from the list!
    /* The remaining mutexes/conds must stay alive until the processor
     * has been removed from the processor list, since the request
     * mechanism accesses them.
     */

    chiCondDestroy(&proc->secondary.cond);
    chiMutexDestroy(&proc->secondary.mutex);
    chiMutexDestroy(&proc->suspend.mutex);
    chiCondDestroy(&proc->suspend.cond);
    chiTaskClose(proc->primary.task);
#endif
}

static void exitProcessor(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    if (chiProcessorMain(proc)) {
        rt->timeRef.end = chiTime();
        CHI_IF(CHI_SYSTEM_HAS_TASK, waitProcessors(proc));
        destroyProcessor(proc);
        chiRuntimeDestroy(proc);
    }
#if CHI_SYSTEM_HAS_TASK
    destroyProcessor(proc);
    chiTaskExit();
#endif
}

void chiProcessorProtectEnd(ChiProcessor* proc) {
    int e = CHI_AND(CHI_SYSTEM_HAS_ERRNO, errno);
    CHI_IF(CHI_SYSTEM_HAS_TASK, secondaryDeactivate(proc));
    chiThreadSetErrno(proc, e);
    if (chiTriggered(&proc->trigger.exit) || chiTriggered(&proc->trigger.userInterrupt))
        chiTrigger(&proc->trigger.interrupt, true);
}

static void serviceExit(ChiProcessor* proc) {
    if (CHI_SYSTEM_HAS_TASK && proc->worker == &proc->primary.worker && chiTriggered(&proc->trigger.exit))
        exitProcessor(proc);
}

static void serviceDumpStack(ChiProcessor* proc) {
    if (chiTriggered(&proc->trigger.dumpStack)) {
        chiTrigger(&proc->trigger.dumpStack, false);
        if (chiEventEnabled(proc, STACK_TRACE)) {
            CHI_STRING_SINK(sink);
            chiStackActivate(proc);
            Chili stack = chiThreadStack(proc->thread);
            Chili* sp = chiToStack(stack)->sp;
            chiStackDump(sink, proc, sp);
            chiEvent(proc, STACK_TRACE, .stack = chiAddress(stack), .trace = chiSinkString(sink));
            chiStackDeactivate(proc, sp);
        }
    }
}

void chiProcessorService(ChiProcessor* proc) {
    chiEvent0(proc, PROC_SERVICE_BEGIN);

    while (chiTriggered(&proc->trigger.interrupt)) {
        chiTrigger(&proc->trigger.interrupt, false);
        chiGCService(proc);
        serviceExit(proc);
        serviceDumpStack(proc);
    }

    chiEvent0(proc, PROC_SERVICE_END);
}

void chiProtectBegin(Chili* sp) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiStackDeactivate(proc, sp);
    chiEvent0(proc, THREAD_RUN_END);
    CHI_IF(CHI_SYSTEM_HAS_TASK, secondaryActivate(proc));
    CHI_IF(CHI_SYSTEM_HAS_ERRNO, errno = 0);
}

void chiProcessorSuspend(Chili handle, uint32_t ms) {
    ChiProcessor* proc = (ChiProcessor*)chiToPtr(handle);
    chiEvent(CHI_CURRENT_PROCESSOR, PROC_SUSPEND, .suspendWid = proc->primary.worker.wid, .ms = ms);

    CHI_LOCK_MUTEX(&proc->suspend.mutex);
    ChiMillis old = proc->suspend.timeout;
    proc->suspend.timeout = chiMillis(ms);
    if (!ms && !chiMillisZero(old))
        chiCondSignal(&proc->suspend.cond);
}

void chiProcessorSetup(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    chiMutexInit(&rt->proc.mutex);
    chiCondInit(&rt->proc.cond);
    CHI_IF(CHI_SYSTEM_HAS_TASK, chiProcessorListInit(&rt->proc.list));
    if (!initProcessor(proc))
        chiSysErr("chiTaskTryCreate");
}

void chiProcessorRequestAll(ChiRuntime* rt, ChiProcessorRequest request) {
    CHI_LOCK_MUTEX(&rt->proc.mutex);
    CHI_FOREACH_PROCESSOR (proc, rt)
        chiProcessorRequest(proc, request);
}

void chiProcessorRequestMain(ChiRuntime* rt, ChiProcessorRequest request) {
    CHI_LOCK_MUTEX(&rt->proc.mutex);
    CHI_FOREACH_PROCESSOR (proc, rt) {
        if (chiProcessorMain(proc)) {
            chiProcessorRequest(proc, request);
            break;
        }
    }
}

void chiExit(uint8_t code) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;
    chiEvent0(proc, THREAD_RUN_END);
    chiStackDeactivate(proc, 0);
    chiEvent0(rt, SHUTDOWN_BEGIN);
    rt->exitCode = code;
    chiProcessorRequestAll(rt, CHI_REQUEST_EXIT);
    exitProcessor(proc);
}

void chiWorkerBegin(ChiWorker* worker, ChiRuntime* rt, const char* prefix) {
    CHI_ASSERT(!worker->rt);
    CHI_ZERO_STRUCT(worker);
    workerBegin(worker, rt, prefix,
                CHI_AND(CHI_SYSTEM_HAS_TASK, atomic_fetch_add_explicit(&rt->nextWid, 1, memory_order_relaxed)));
}

void chiWorkerEnd(ChiWorker* worker) {
    chiHookRun(CHI_HOOK_WORKER_STOP, worker);
    chiEventWorkerStop(worker);
}

ChiProcessor* chiProcessorBegin(ChiRuntime* rt) {
#if CHI_SYSTEM_HAS_TASK
    ChiProcessor* proc = &_chiProcessor;
    CHI_ZERO_STRUCT(proc);
    proc->worker = &proc->primary.worker;
    uint32_t wid = atomic_fetch_add_explicit(&rt->nextWid, 2, memory_order_relaxed);
#else
    _chiRuntime = rt;
    ChiProcessor* proc = &rt->proc.single;
    uint32_t wid = 0;
#endif
    CHI_ASSERT(!proc->rt);
    proc->rt = rt;
    workerBegin(proc->worker, rt, "proc", wid);
    chiEvent0(proc, PROC_INIT);
    return proc;
}
