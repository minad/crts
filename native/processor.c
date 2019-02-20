#include "event.h"
#include "private.h"
#include "runtime.h"
#include "sink.h"
#include "stack.h"
#include "thread.h"
#include "trace.h"

#define PROC_TRANSITION(p, a, b, doc)     ({ CHI_ASSERT((p)->state.id == CHI_PROCESSOR_##a); (p)->state.id = CHI_PROCESSOR_##b; ({}); })
#define PM_TRANSITION(pm, a, b, doc) ({ CHI_ASSERT((pm)->state == CHI_PM_##a); (pm)->state = CHI_PM_##b; ({}); })
#define ATOMIC_PM_TRANSITION(pm, a, b, doc)                        \
    ({                                                                  \
        ChiPMState _a = CHI_PM_##a;                       \
        atomic_compare_exchange_strong(&(pm)->state, &_a, CHI_PM_##b); \
    })

_Thread_local ChiProcessor _chiProcessor;

static void interruptProcessor(ChiProcessor* proc) {
    proc->trigger.interrupt = true;
    if (proc->trigger.hl)
        *proc->trigger.hl = 0; // Force scheduling
}

static bool resumeProcessor(ChiProcessor* proc) {
    CHI_LOCK(&proc->suspend.mutex);
    if (!chiMillisZero(proc->suspend.timeout)) {
        proc->suspend.timeout = (ChiMillis){0};
        chiCondSignal(&proc->suspend.cond);
        return true;
    }
    return false;
}

static uint32_t resumeProcessors(ChiProcessor* proc, uint32_t count) {
    CHI_FOREACH_OTHER_PROCESSOR (p, proc) {
        if (!count)
            break;
        if (resumeProcessor(p))
            --count;
    }
    return count;
}

static void startOnDemand(ChiRuntime* rt, uint32_t count) {
    ChiProcessorManager* pm = &rt->procMan;
    while (count) {
        uint32_t running = pm->running;
        if (running >= rt->option.processors)
            break;
        if (!atomic_compare_exchange_strong(&pm->running, &running, running + 1))
            continue;
        if (!chiProcessorTryStart(rt)) {
            --pm->running;
            break;
        }
        --pm->running;
        --count;
    }
}

static void waitSync(ChiProcessor* proc) {
    CHI_EVENT0(proc, PROC_WAIT_SYNC_BEGIN);

    ChiProcessorManager* pm = &proc->rt->procMan;
    ChiProcessor* head;
    do {
        head = pm->list.head;
        CHI_FOREACH_OTHER_PROCESSOR_HEAD(p, proc, head) {
            CHI_LOCK(&p->state.mutex);
            while (p->state.id != CHI_PROCESSOR_SYNC &&
                   p->state.id != CHI_PROCESSOR_STOP) {
                if (p->state.id == CHI_PROCESSOR_PROTECT) {
                    PROC_TRANSITION(p, PROTECT, SYNC, "e=wait sync");
                } else {
                    interruptProcessor(p);
                    chiCondWait(&p->state.cond, &p->state.mutex);
                }
            }
        }
    } while (head != pm->list.head); // again if list was modified

    CHI_EVENT0(proc, PROC_WAIT_SYNC_END);

    CHI_TT(&proc->worker, .name = "runtime;wait sync");
}

static void initProcessor(ChiProcessor* proc) {
    proc->task = chiTaskCurrent();
    chiMutexInit(&proc->state.mutex);
    chiMutexInit(&proc->suspend.mutex);
    chiCondInit(&proc->suspend.cond);

    ChiProcessorManager* pm = &proc->rt->procMan;
    ++pm->running;

    ChiProcessor* oldHead;
    do {
        oldHead = pm->list.head;
        proc->next = oldHead;
    } while (!atomic_compare_exchange_weak(&pm->list.head, &oldHead, proc));

    chiHeapHandleInit(&proc->heapHandle, &proc->rt->heap);
    const ChiRuntimeOption* opt = &proc->rt->option;
    chiNurserySetup(&proc->nursery, opt->block.size, opt->block.chunk, opt->block.nursery);
    chiGCProcStart(proc);

    proc->thread = chiThreadNewUninitialized(proc);
    proc->schedulerThread = chiThreadNewUninitialized(proc);
    chiThreadSetExceptionBlock(proc->schedulerThread, 1);
    chiThreadSetNameField(proc, proc->schedulerThread, chiFmtString("sched%u", proc->worker.wid));
}

typedef struct {
    ChiMutex    mutex;
    ChiCond     cond;
    ChiRuntime* rt;
    uint32_t    wid;
} ProcessorArg;

static void processorTask(void* _arg) {
    ProcessorArg* arg = (ProcessorArg*)_arg;
    ChiProcessor* proc = chiProcessorBegin(arg->rt);

    initProcessor(proc);
    // first thread is a dummy thread which is terminated
    chiThreadSetStateField(proc, proc->thread, chiNewEmpty(CHI_TAG(CHI_TS_TERMINATED)));
    {
        CHI_LOCK(&arg->mutex);
        arg->wid = proc->worker.wid;
        chiCondSignal(&arg->cond);
    }
    chiEnter();
}

uint32_t chiProcessorTryStart(ChiRuntime* rt) {
    ProcessorArg arg = { .rt = rt };
    chiMutexInit(&arg.mutex);
    chiCondInit(&arg.cond);
    ChiTask t = chiTaskTryCreate(processorTask, &arg);
    if (!chiTaskNull(t)) {
        chiTaskClose(t);
        // Processors are started synchronously
        // This ensures that all processors are registered, when the stop the world sync happens.
        CHI_LOCK(&arg.mutex);
        while (!arg.wid)
            chiCondWait(&arg.cond, &arg.mutex);
    }
    chiMutexDestroy(&arg.mutex);
    chiCondDestroy(&arg.cond);
    return arg.wid;
}

static void finalizeProcessors(ChiProcessorManager* pm) {
    _Atomic(ChiProcessor*) *prev = &pm->list.head;
    while (*prev) {
        ChiProcessor* p = *prev;
        if (p->state.id == CHI_PROCESSOR_STOP) {
            *prev = p->next;
            p->nextStopped = pm->list.stopped;
            pm->list.stopped = p;
        } else {
            prev = &p->next;
        }
    }

    // We delay really deleting the processors until
    // the list is non critical
    if (!pm->list.critical) {
        while (pm->list.stopped) {
            --pm->running;
            ChiProcessor* p = pm->list.stopped;
            pm->list.stopped = p->nextStopped;
            ChiTask task = p->task;
            {
                CHI_LOCK(&p->state.mutex);
                PROC_TRANSITION(p, STOP, FINAL, "e=finalize");
                chiCondSignal(&p->state.cond);
            }
            chiTaskCancel(task);
            chiTaskJoin(task);
        }
    }
}

static void unsyncAll(ChiProcessor* proc) {
    CHI_FOREACH_OTHER_PROCESSOR (p, proc) {
        CHI_LOCK(&p->state.mutex);
        if (p->state.id == CHI_PROCESSOR_SYNC) {
            PROC_TRANSITION(p, SYNC, PROTECT, "e=unsync");
            chiCondSignal(&p->state.cond);
        } else {
            CHI_ASSERT(p->state.id == CHI_PROCESSOR_STOP);
        }
    }
}

static void destroyProcessor(ChiProcessor* proc) {
    CHI_EVENT0(proc, PROC_RUN_END);
    chiHookRun(&proc->worker, CHI_HOOK_PROC_STOP);
    CHI_EVENT0(proc, PROC_DESTROY);
    chiWorkerEnd(&proc->worker);
    chiGCProcStop(proc);
    chiNurseryDestroy(&proc->nursery);
    chiMutexDestroy(&proc->state.mutex);
    chiMutexDestroy(&proc->suspend.mutex);
    chiCondDestroy(&proc->suspend.cond);
}

static void synced(ChiProcessor* proc) {
    CHI_ASSERT(proc->state.id == CHI_PROCESSOR_SYNC);

    chiCondSignal(&proc->state.cond);

    CHI_EVENT0(proc, PROC_RUN_END);
    CHI_EVENT0(proc, PROC_SYNC_BEGIN);

    while (proc->state.id == CHI_PROCESSOR_SYNC)
        chiCondWait(&proc->state.cond, &proc->state.mutex);

    CHI_TT(&proc->worker, .name = "runtime;sync");

    CHI_EVENT0(proc, PROC_SYNC_END);
    CHI_EVENT0(proc, PROC_RUN_BEGIN);

    PROC_TRANSITION(proc, PROTECT, RUN, "e=unprotect");
}

static void syncProcessor(ChiProcessor* proc) {
    CHI_LOCK(&proc->state.mutex);
    PROC_TRANSITION(proc, RUN, PROTECT, "e=protect");
    PROC_TRANSITION(proc, PROTECT, SYNC, "e=sync");
    synced(proc);
}

static void protectSynced(ChiProcessor* proc) {
    CHI_LOCK(&proc->state.mutex);

    CHI_ASSERT(proc->state.id == CHI_PROCESSOR_PROTECT
               || proc->state.id == CHI_PROCESSOR_SYNC);

    if (proc->state.id == CHI_PROCESSOR_PROTECT) {
        if (proc->rt->procMan.state != CHI_PM_SYNC) {
            PROC_TRANSITION(proc, PROTECT, RUN, "e=unprotect");
            return;
        }
        PROC_TRANSITION(proc, PROTECT, SYNC, "e=sync");
    }

    synced(proc);
}

void chiProcessorProtectEnd(ChiProcessor* proc) {
    int e = CHI_IFELSE(CHI_SYSTEM_HAS_ERRNO, errno, 0);
    protectSynced(proc);
    chiStackActivate(proc);
    chiThreadSetErrno(proc->thread, e);
    CHI_THREAD_EVENT(BEGIN, proc);
}

/**
 * This function is called by processors to
 * synchronize between them. This is needed in particular
 * to initiate a stop-the-world pause for the garbage collector
 * to run.
 */
bool chiProcessorSync(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    ChiProcessorManager* pm = &rt->procMan;

    do {
        if (pm->state == CHI_PM_SYNC)
            syncProcessor(proc);

        if (rt->exitCode >= 0) {
            if (!chiMainWorker(&proc->worker))
                chiProcessorStopped(proc);

            if (pm->running == 1) {
                PM_TRANSITION(pm, RUN, FINAL, "");
                rt->timeRef.end = chiTime();
                chiHeapHandleRelease(&proc->heapHandle);
                destroyProcessor(proc);
                chiDestroy(rt);
                return false;
            }

            rt->gc.trigger = CHI_GC_TRIGGER_SLICE;
            resumeProcessors(proc, ~0U);
            chiTaskYield(); // give the other processors some time
        }

        if (rt->gc.trigger && ATOMIC_PM_TRANSITION(pm, RUN, SYNC, "t=PM")) {
            CHI_EVENT0(proc, PROC_RUN_END);
            CHI_EVENT0(proc, PROC_SYNC_BEGIN);

            waitSync(proc);

            if (rt->exitCode < 0) {
                chiGCSlice(proc, rt->gc.trigger);
                rt->gc.trigger = CHI_GC_TRIGGER_INACTIVE;
            }

            finalizeProcessors(pm);

            CHI_EVENT0(proc, PROC_SYNC_END);
            CHI_EVENT0(proc, PROC_RUN_BEGIN);
            PM_TRANSITION(pm, SYNC, UNSYNC, "");
            unsyncAll(proc);
            PM_TRANSITION(pm, UNSYNC, RUN, "");
        }
    } while (rt->exitCode >= 0 || rt->gc.trigger);

    return true;
}

void chiProtectBegin(Chili* sp, ChiWord* hp) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    chiNurserySetPtr(&proc->nursery, hp);
    chiStackDeactivate(proc, sp);
    CHI_THREAD_EVENT(END, proc);
    {
        CHI_LOCK(&proc->state.mutex);
        PROC_TRANSITION(proc, RUN, PROTECT, "e=protect");
        chiCondSignal(&proc->state.cond);
    }
    CHI_IF(CHI_SYSTEM_HAS_ERRNO, errno = 0);
}

void chiProcessorResume(uint32_t count) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_EVENT(proc, PROC_RESUME_REQ, .count = count);
    startOnDemand(proc->rt, resumeProcessors(proc, count));
}

void chiProcessorSetup(ChiProcessor* proc) {
    PM_TRANSITION(&proc->rt->procMan, INITIAL, RUN, "");
    initProcessor(proc);
}

enum {
    INTERRUPT_TICK      = 1,
    INTERRUPT_DUMPSTACK = 2,
    INTERRUPT_RESUME    = 4
};

static void interruptProcessors(ChiRuntime* rt, uint32_t ints) {
    ++rt->procMan.list.critical;
    CHI_FOREACH_PROCESSOR (proc, rt) {
        if (ints & INTERRUPT_TICK)
            proc->trigger.tick = true;
        if (ints & INTERRUPT_DUMPSTACK)
            proc->trigger.dumpStack = true;
        if (ints & INTERRUPT_RESUME)
            resumeProcessor(proc);
        interruptProcessor(proc);
    }
    --rt->procMan.list.critical;
}

void chiProcessorInterrupt(ChiRuntime* rt) {
    interruptProcessors(rt, 0);
}

void chiProcessorTick(ChiRuntime* rt) {
    interruptProcessors(rt, INTERRUPT_TICK);
}

void chiProcessorSignal(ChiRuntime* rt, ChiSig sig) {
    interruptProcessors(rt, INTERRUPT_RESUME |
                        (sig == CHI_SIG_DUMPSTACK ? INTERRUPT_DUMPSTACK : 0));
}

uint32_t chiProcessorId(void) {
    return CHI_CURRENT_PROCESSOR->worker.wid;
}

_Noreturn void chiProcessorStopped(ChiProcessor* proc) {
    CHI_ASSERT(!chiMainWorker(&proc->worker));
    chiHeapHandleRelease(&proc->heapHandle);

    {
        CHI_LOCK(&proc->state.mutex);
        PROC_TRANSITION(proc, RUN, STOP, "e=stop");
        chiCondSignal(&proc->state.cond);
        while (proc->state.id != CHI_PROCESSOR_FINAL)
            chiCondWait(&proc->state.cond, &proc->state.mutex);
    }

    destroyProcessor(proc);
    chiTaskExit();
}

ChiWorker* chiWorkerBegin(ChiRuntime* rt, const char* prefix) {
    ChiWorker* worker = &_chiProcessor.worker;
    CHI_ASSERT(!worker->rt);
    worker->rt = rt;
    worker->wid = rt->nextWid++;
    worker->tp = &worker->_tp;
    chiFmt(worker->name, sizeof (worker->name), "%s%u", prefix, worker->wid);
    chiTaskName(worker->name);
    chiEventWorkerStart(worker);
    chiHookRun(worker, CHI_HOOK_WORKER_START);
    return worker;
}

void chiWorkerEnd(ChiWorker* worker) {
    chiHookRun(worker, CHI_HOOK_WORKER_STOP);
    chiEventWorkerStop(worker);
}

ChiProcessor* chiProcessorBegin(ChiRuntime* rt) {
    chiWorkerBegin(rt, "proc");
    ChiProcessor* proc = &_chiProcessor;
    CHI_ASSERT(!proc->rt);
    proc->rt = rt;
    CHI_EVENT0(proc, PROC_INIT);
    return proc;
}
