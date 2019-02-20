#include "runtime.h"
#include "event.h"
#include "mem.h"
#include "heapcheck.h"
#include "heapprof.h"
#include "heapdump.h"
#include "gc.h"
#include "scavenger.h"
#include "num.h"
#include "sink.h"
#include "error.h"
#include "mark.h"
#include "trace.h"
#include "timeout.h"

#define VEC_TYPE Chili
#define VEC_PREFIX chiRootVec
#define VEC ChiRootVec
#define NOVEC
#include "vector.h"

_Static_assert(!CHI_GC_CONC_ENABLED || CHI_SYSTEM_HAS_TASKS, "Concurrent GC requires task support!");

#define WORKER_TRANSITION(w, a, b, doc) ({ CHI_ASSERT(w->state == WORKER_##a); w->state = WORKER_##b; ({}); })

/**
 * GC worker states
 *
 * @image html gc-statemachine.png
 */
typedef enum {
    WORKER_INITIAL,
    WORKER_FINAL,
    WORKER_WAIT,
    WORKER_RUN,
    WORKER_STOP,
    WORKER_SUSPEND,
} WorkerState;

typedef struct {
    ChiRuntime* rt;
    ChiTask     task;
    ChiTimeout  timeout;
    WorkerState state;
} Worker;

typedef struct {
    ChiMutex mutex;
    ChiCond  cond;
    uint32_t numWorkers;
    Worker   worker[0];
} WorkerManager;

struct ChiGCSweeper_ {
    WorkerManager manager;
};

struct ChiGCMarker_ {
    ChiBlockVec grayList;
    WorkerManager manager;
};

CHI_INL bool hasMarker(ChiGC* gc) {
    return CHI_GC_CONC_ENABLED && gc->conc->marker;
}

CHI_INL bool hasSweeper(ChiGC* gc) {
    return CHI_GC_CONC_ENABLED && gc->conc->sweeper;
}

static void workerManagerSetup(WorkerManager* wm, ChiTaskRun run, uint32_t numWorkers, ChiRuntime* rt) {
    chiMutexInit(&wm->mutex);
    chiCondInit(&wm->cond);
    wm->numWorkers = numWorkers;
    for (uint32_t i = 0; i < numWorkers; ++i) {
        Worker* w = wm->worker + i;
        w->rt = rt;
        WORKER_TRANSITION(w, INITIAL, WAIT, "t=MARKER/SWEEPER");
        w->task = chiTaskCreate(run, w);
    }
}

static void workerStop(WorkerManager* wm) {
    CHI_LOCK(&wm->mutex);
    for (Worker* w = wm->worker; w < wm->worker + wm->numWorkers; ++w) {
        if (w->state == WORKER_WAIT)
            WORKER_TRANSITION(w, WAIT, STOP, "t=MARKER/SWEEPER,e=stop");
        else
            WORKER_TRANSITION(w, RUN, STOP, "t=MARKER/SWEEPER,e=stop");
        chiTimeoutTrigger(&w->timeout, true);
    }
    chiCondBroadcast(&wm->cond);
}

static void workerManagerDestroy(WorkerManager* wm) {
    workerStop(wm);
    for (uint32_t i = 0; i < wm->numWorkers; ++i)
        chiTaskJoin(wm->worker[i].task);
    chiCondDestroy(&wm->cond);
    chiMutexDestroy(&wm->mutex);
}

static bool workerWaiting(WorkerManager* wm) {
    for (Worker* w = wm->worker; w < wm->worker + wm->numWorkers; ++w) {
        CHI_ASSERT(w->state != WORKER_STOP);
        if (w->state != WORKER_WAIT)
            return false;
    }
    return true;
}

static bool workerSuspend(WorkerManager* wm) {
    bool running = !workerWaiting(wm);
    if (running) {
        for (Worker* w = wm->worker; w < wm->worker + wm->numWorkers; ++w) {
            if (w->state == WORKER_RUN) {
                WORKER_TRANSITION(w, RUN, SUSPEND, "t=MARKER/SWEEPER,e=gc req");
                chiTimeoutTrigger(&w->timeout, true);
            } else {
                CHI_ASSERT(w->state == WORKER_WAIT);
            }
        }
        chiCondBroadcast(&wm->cond);
        while (!workerWaiting(wm))
            chiCondWait(&wm->cond, &wm->mutex);
    }
    return running;
}

static void workerNotify(WorkerManager* wm, bool strict) {
    for (Worker* w = wm->worker; w < wm->worker + wm->numWorkers; ++w) {
        if (strict || w->state == WORKER_WAIT) // strict state checking
            WORKER_TRANSITION(w, WAIT, RUN, "t=MARKER/SWEEPER,e=gc notify");
    }
    chiCondBroadcast(&wm->cond);
}

static ChiGen computeGCGen(uint32_t* gcTrip, uint32_t heapGen, uint32_t multiplicity, bool fullScavenge) {
    if (fullScavenge) {
        *gcTrip = 0;
        return (ChiGen)(heapGen - 1);
    }
    ++*gcTrip;
    for (uint32_t n = heapGen; n --> 1;) {
        if (*gcTrip % chiPow(multiplicity, n) == 0)
            return (ChiGen)n;
    }
    return CHI_GEN_NURSERY;
}

static void heapCheck(ChiProcessor* proc, ChiHeapCheck mode) {
    if (CHI_AND(CHI_HEAP_CHECK_ENABLED, proc->rt->option.heap.check)) {
        CHI_EVENT0(proc, HEAP_CHECK_BEGIN);
        chiHeapCheck(proc->rt, mode);
        CHI_EVENT0(proc, HEAP_CHECK_END);
        CHI_TT(&proc->worker, .name = "runtime;heap check");
    }
}

static void sweepDirtyLists(ChiProcessor* proc) {
    // All dirty lists are located on the current processor
    ChiBlockVec newDirty, *dirty = proc->gcPS.dirty;
    chiBlockVecInit(&newDirty, &proc->rt->tenure.manager);
    Chili c;
    while (chiBlockVecPop(dirty, &c)) {
        CHI_ASSERT(chiMajor(c));
        ChiObject* obj = chiObject(c);
        ChiColor color = chiObjectColor(obj);
        CHI_ASSERT(chiObjectDirty(obj));
        CHI_ASSERT(color != CHI_GRAY);
        if (color == CHI_BLACK)
            chiBlockVecPush(&newDirty, c);
    }
    chiBlockVecJoin(dirty, &newDirty);
}

static void markerResume(ChiProcessor* proc, ChiGCMarker* marker) {
    WorkerManager* wm = &marker->manager;
    CHI_LOCK(&wm->mutex);
    chiBlockVecJoin(&marker->grayList, &proc->gcPS.grayList);
    workerNotify(wm, true);
}

static void markSlice(ChiWorker* worker, ChiBlockVec* grayList, ChiTimeout* timeout) {
    CHI_EVENT0(worker, GC_MARK_SLICE_BEGIN);

    ChiScanStats stats;
    const ChiRuntimeOption* opt = &worker->rt->option;
    chiScan(grayList, timeout, &stats, opt->heap.scanDepth, opt->gc.ms.noCollapse);

    CHI_TT(worker, .name = "runtime;gc;mark");
    CHI_EVENT_STRUCT(worker, GC_MARK_STATS, &stats);
    CHI_EVENT0(worker, GC_MARK_SLICE_END);
}

static void sweepSlice(ChiWorker* worker, ChiTimeout* timeout) {
    ChiRuntime* rt = worker->rt;
    CHI_EVENT0(worker, GC_SWEEP_SLICE_BEGIN);
    ChiSweepStats stats;
    chiSweepSlice(&rt->heap, &stats, timeout);
    CHI_EVENT_STRUCT(worker, GC_SWEEP_STATS, &stats);
    CHI_EVENT0(worker, GC_SWEEP_SLICE_END);
    CHI_TT(worker, .name = "runtime;gc;sweep");
}

static bool sweeperSync(Worker* w) {
    ChiGCSweeper* sweeper = w->rt->gc.conc->sweeper;
    CHI_LOCK(&sweeper->manager.mutex);
    for (;;) {
        if (w->state == WORKER_RUN)
            WORKER_TRANSITION(w, RUN, WAIT, "t=SWEEPER,e=sw done");

        while (w->state == WORKER_WAIT)
            chiCondWait(&sweeper->manager.cond, &sweeper->manager.mutex);

        if (w->state == WORKER_STOP) {
            WORKER_TRANSITION(w, STOP, FINAL, "t=SWEEPER");
            return false;
        }

        if (w->state == WORKER_SUSPEND) {
            WORKER_TRANSITION(w, SUSPEND, WAIT, "t=SWEEPER,e=sw suspend");
            chiTimeoutTrigger(&w->timeout, false);
            chiCondBroadcast(&sweeper->manager.cond);
            continue;
        }

        CHI_ASSERT(w->state == WORKER_RUN);
        return true;
    }
}

static void sweeperRun(void* arg) {
    Worker* w = (Worker*)arg;
    ChiWorker* worker = chiWorkerBegin(w->rt, "gc-sweep");
    while (sweeperSync(w)) {
        CHI_TT(worker, .name = "runtime;suspend");
        sweepSlice(worker, &w->timeout);
    }
    chiWorkerEnd(worker);
}

static void sweeperNotify(ChiGCSweeper* sweeper) {
    WorkerManager* wm = &sweeper->manager;
    CHI_LOCK(&wm->mutex);
    workerNotify(wm, false);
}

static void sweeperSuspend(ChiGCSweeper* sweeper) {
    WorkerManager* wm = &sweeper->manager;
    CHI_LOCK(&wm->mutex);
    workerSuspend(wm);
}

static void sweeperSetup(ChiRuntime* rt) {
    ChiRuntimeOption* opt = &rt->option;
    if (CHI_AND(CHI_GC_CONC_ENABLED, opt->gc.mode == CHI_GC_CONC)
        && !rt->gc.conc->sweeper && opt->gc.conc->sweepers) {
        ChiGCSweeper* sweeper = rt->gc.conc->sweeper =
            (ChiGCSweeper*)chiZalloc(sizeof (ChiGCSweeper) + opt->gc.conc->sweepers * sizeof (Worker));
        workerManagerSetup(&sweeper->manager, sweeperRun, opt->gc.conc->sweepers, rt);
    }
}

static void heapRelease(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    ChiHeap* heap = &rt->heap;
    bool notify = false;
    {
        CHI_LOCK(&heap->mutex);
        CHI_FOREACH_PROCESSOR (p, rt)
            notify = chiHeapHandleReleaseUnlocked(&p->heapHandle) || notify;
        notify = notify && heap->sweep;
    }
    if (notify)
        chiHeapSweepNotify(heap);
}

static void markPhase(ChiProcessor* proc, ChiTimeout* timeout, bool finishMark) {
    ChiRuntime* rt = proc->rt;
    ChiGC* gc = &rt->gc;

    ChiBlockVec grayList;
    chiBlockVecInit(&grayList, &rt->blockTemp);
    CHI_FOREACH_PROCESSOR (p, rt)
        chiBlockVecJoin(&grayList, &p->gcPS.grayList);

    if (finishMark) {
        for (size_t i = 0; i < gc->roots.used; ++i) {
            Chili root = gc->roots.elem[i];
            ChiObject* obj = chiObject(root);
            if (chiRaw(chiType(root))) {
                chiObjectSetColor(obj, CHI_BLACK);
            } else if (chiObjectCasColor(obj, CHI_WHITE, CHI_GRAY)) {
                chiBlockVecPush(&grayList, root);
            }
        }
    }

    if ((finishMark || !hasMarker(gc)) && chiTimeoutTick(timeout) && !chiBlockVecNull(&grayList))
        markSlice(&proc->worker, &grayList, timeout);

    chiBlockVecJoin(&proc->gcPS.grayList, &grayList);

    if (finishMark && chiBlockVecNull(&proc->gcPS.grayList)) {
        CHI_ASSERT(gc->msPhase == CHI_MS_PHASE_MARK);
        heapCheck(proc, CHI_HEAPCHECK_PHASECHANGE);
        sweeperSetup(rt);
        gc->msPhase = CHI_MS_PHASE_SWEEP;
        sweepDirtyLists(proc);
        CHI_EVENT0(rt, GC_MARK_PHASE_END);
        CHI_EVENT0(rt, GC_SWEEP_PHASE_BEGIN);

        heapRelease(proc);
        chiSweepBegin(&rt->heap);
    }
}

static void sweeperDestroy(ChiGCSweeper* sweeper) {
    workerManagerDestroy(&sweeper->manager);
    chiFree(sweeper);
}

static bool markerSync(Worker* w, ChiBlockVec* grayList) {
    ChiGCMarker* marker = w->rt->gc.conc->marker;

    CHI_LOCK(&marker->manager.mutex);
    for (;;) {
        chiBlockVecJoin(&marker->grayList, grayList);

        while (w->state == WORKER_WAIT)
            chiCondWait(&marker->manager.cond, &marker->manager.mutex);

        if (w->state == WORKER_STOP) {
            WORKER_TRANSITION(w, STOP, FINAL, "t=MARKER");
            return false;
        }

        if (w->state == WORKER_SUSPEND) {
            WORKER_TRANSITION(w, SUSPEND, WAIT, "t=MARKER,e=mark suspend");
            chiTimeoutTrigger(&w->timeout, false);
            chiCondBroadcast(&marker->manager.cond);
            continue;
        }

        CHI_ASSERT(w->state == WORKER_RUN);

        if (chiBlockVecNull(&marker->grayList)) {
            WORKER_TRANSITION(w, RUN, WAIT, "t=MARKER,e=mark done");
            continue;
        }

        chiBlockVecMove(grayList, &marker->grayList);
        return true;
    }
}

static void markerRun(void* arg) {
    Worker* w = (Worker*)arg;
    ChiRuntime* rt = w->rt;

    ChiWorker* worker = chiWorkerBegin(rt, "gc-mark");

    ChiBlockVec grayList;
    chiBlockVecInit(&grayList, &rt->blockTemp);

    while (markerSync(w, &grayList)) {
        CHI_TT(worker, .name = "runtime;suspend");
        markSlice(worker, &grayList, &w->timeout);
    }

    chiBlockVecFree(&grayList);
    chiWorkerEnd(worker);
}

static void markerSetup(ChiRuntime* rt) {
    ChiRuntimeOption* opt = &rt->option;
    if (CHI_AND(CHI_GC_CONC_ENABLED, opt->gc.mode == CHI_GC_CONC)
        && !rt->gc.conc->marker && opt->gc.conc->markers) {
        ChiGCMarker* marker = rt->gc.conc->marker =
            (ChiGCMarker*)chiZalloc(sizeof (ChiGCMarker) + opt->gc.conc->markers * sizeof (Worker));
        chiBlockVecInit(&marker->grayList, &rt->blockTemp);
        workerManagerSetup(&marker->manager, markerRun, opt->gc.conc->markers, rt);
    }
}

static void markerDestroy(ChiGCMarker* marker) {
    workerManagerDestroy(&marker->manager);
    chiBlockVecFree(&marker->grayList);
    chiFree(marker);
}

static void minorStats(ChiProcessor* proc, ChiMinorUsage* stats) {
    ChiRuntime* rt = proc->rt;
    const ChiRuntimeOption* opt = &proc->rt->option;
    size_t
        usedBlocks = rt->blockTemp.blocksUsed + rt->tenure.manager.blocksUsed,
        freeBlocks = rt->blockTemp.free.count + rt->tenure.manager.free.count;

    CHI_FOREACH_PROCESSOR (p, rt) {
        usedBlocks += p->nursery.manager.blocksUsed;
        freeBlocks += p->nursery.manager.free.count;
    }

    stats->usedWords = opt->block.size * usedBlocks / CHI_WORDSIZE;
    stats->totalWords = opt->block.size * (usedBlocks + freeBlocks) / CHI_WORDSIZE;
}

static void heapStats(ChiProcessor* proc, ChiEventHeapUsage* stats) {
    minorStats(proc, &stats->minor);
    chiHeapUsage(&proc->rt->heap, &stats->major);
    stats->totalWords = proc->rt->heapSize / CHI_WORDSIZE;
}

static void nurseryResize(ChiProcessor* proc, ChiNanos begin, ChiNanos end) {
    ChiRuntime* rt = proc->rt;
    const ChiRuntimeOption* opt = &rt->option;
    if (!chiNanosZero(end)) {
        size_t
            oldLimit = proc->nursery.manager.blocksLimit,
            newLimit = CHI_CLAMP((size_t)(oldLimit
                                          * CHI_UN(Nanos, chiMicrosToNanos(opt->gc.scav.slice))
                                          / CHI_UN(Nanos, chiNanosDelta(end, begin))),
                                 2, opt->block.nursery);
        if (oldLimit != newLimit) {
            CHI_FOREACH_PROCESSOR(p, rt)
                p->nursery.manager.blocksLimit = newLimit;
            CHI_EVENT(proc, NURSERY_RESIZE, .newLimit = newLimit, .oldLimit = oldLimit);
        }
    }
}

static void scavenger(ChiProcessor* proc, ChiGen gcGen, bool snapshot) {
    ChiRuntime* rt = proc->rt;
    const ChiRuntimeOption* opt = &rt->option;

    if (CHI_EVENT_P(proc, HEAP_BEFORE_SCAV)) {
        ChiEventHeapUsage e;
        heapStats(proc, &e);
        CHI_EVENT_STRUCT(proc, HEAP_BEFORE_SCAV, &e);
    }

    ChiScavengerStats stats;
    CHI_EVENT0(proc, GC_SCAVENGER_BEGIN);

    ChiNanos begin = chiMicrosZero(opt->gc.scav.slice) || gcGen > CHI_GEN_NURSERY
        ? (ChiNanos){0} : chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), rt->timeRef.start.real);
    chiScavenger(proc, gcGen, snapshot, &stats);
    ChiNanos end = chiMicrosZero(opt->gc.scav.slice) || gcGen > CHI_GEN_NURSERY
        ? (ChiNanos){0} : chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), rt->timeRef.start.real);

    CHI_EVENT_STRUCT(proc, GC_SCAVENGER_END, &stats);

    if (CHI_EVENT_P(proc, HEAP_AFTER_SCAV)) {
        ChiEventHeapUsage m;
        heapStats(proc, &m);
        CHI_EVENT_STRUCT(proc, HEAP_AFTER_SCAV, &m);
    }

    nurseryResize(proc, begin, end);
}

/*
 * Scavenger dirty scanning of major objects interacts
 * badly with the concurrent scanning of the marking
 * workers on 32 bit architectures.
 * It could happen that the scanning modifies the object,
 * while it is at the same time scanned by the concurrent
 * marking workers. On 32 bit this will lead to inconsistent
 * reads in the marking workers. Therefore marker threads will
 * be temporarily stopped.
 */
static bool markerSuspend(ChiProcessor* proc, ChiGCMarker* marker) {
    WorkerManager* wm = &marker->manager;
    CHI_LOCK(&wm->mutex);
    bool markerActive = workerSuspend(wm);
    chiBlockVecJoin(&proc->gcPS.grayList, &marker->grayList);
    return markerActive;
}

static void heapProf(ChiProcessor* proc) {
    ChiHeapProf prof = {.box64={0,0}};
    CHI_EVENT0(proc, HEAP_PROF_BEGIN);
    chiHeapProf(proc->rt, &prof);
    CHI_EVENT_STRUCT(proc, HEAP_PROF_END, &prof);
    CHI_TT(&proc->worker, .name = "runtime;heap prof");
}

static void heapDump(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    ChiGC* gc = &rt->gc;

    char filebuf[64], *file = "4";
    if (!CHI_DEFINED(CHI_STANDALONE_SANDBOX)) {
        file = filebuf;
        chiFmt(filebuf, sizeof (filebuf), "heap.%u-%u.json%s",
               chiPid(), gc->dumpId++, chiSinkCompress());
    }

    CHI_AUTO_SINK(sink, chiSinkBufferNew(chiSinkFileTryNew(file, CHI_SINK_BINARY), 1024*1024));
    if (!sink)
        return;
    CHI_EVENT0(proc, HEAP_DUMP_BEGIN);
    chiHeapDump(proc->rt, sink);
    CHI_EVENT(proc, HEAP_DUMP_END, .file = chiStringRef(file));

    gc->dumpHeap = CHI_NODUMP;
    CHI_TT(&proc->worker, .name = "runtime;heap dump");
}

static void markSweepSlice(ChiProcessor* proc, bool fullGC, bool finishMark) {
    ChiRuntime* rt = proc->rt;
    ChiGC* gc = &rt->gc;
    const ChiRuntimeOption* opt = &rt->option;

    CHI_ASSERT(gc->msPhase == CHI_MS_PHASE_SWEEP || CHI_MS_PHASE_MARK);

    ChiTimeout timeout;
    chiTimeoutStart(&timeout, fullGC ? CHI_TIMEOUT_INFINITE : opt->gc.ms.slice);

    if (gc->msPhase == CHI_MS_PHASE_MARK)
        markPhase(proc, &timeout, finishMark);
    else
        heapRelease(proc);

    if (gc->msPhase == CHI_MS_PHASE_SWEEP && chiTimeoutTick(&timeout)) {
        if (hasSweeper(gc) && fullGC)
            sweeperSuspend(gc->conc->sweeper);
        if (!hasSweeper(gc) || fullGC)
            sweepSlice(&proc->worker, &timeout);
    }

    chiTimeoutStop(&timeout);

    bool sweepDone = gc->msPhase == CHI_MS_PHASE_SWEEP && chiSweepEnd(&rt->heap);
    heapCheck(proc, sweepDone ? CHI_HEAPCHECK_PHASECHANGE : CHI_HEAPCHECK_NORMAL);

    if (sweepDone) {
        gc->msPhase = CHI_MS_PHASE_IDLE;
        CHI_EVENT0(rt, GC_SWEEP_PHASE_END);
        CHI_EVENT0(rt, GC_MARKSWEEP_END);
    }
}

void chiGCSlice(ChiProcessor* proc, ChiGCTrigger trigger) {
    ChiRuntime* rt = proc->rt;
    ChiGC* gc = &rt->gc;
    const ChiRuntimeOption* opt = &rt->option;

    CHI_ASSERT(trigger != CHI_GC_TRIGGER_INACTIVE);
    CHI_EVENT0(proc, GC_SLICE_BEGIN);

    const bool
        fullGC = opt->gc.mode == CHI_GC_FULL || trigger == CHI_GC_TRIGGER_FULL,
        initMark = gc->msPhase == CHI_MS_PHASE_IDLE && opt->gc.mode != CHI_GC_NOMS && (fullGC || trigger == CHI_GC_TRIGGER_MARKSWEEP);

    bool finishMark;
    ChiGen gcGen;
    if (hasMarker(gc)) {
        bool markerActive = gc->msPhase == CHI_MS_PHASE_MARK && markerSuspend(proc, gc->conc->marker);
        finishMark = gc->msPhase == CHI_MS_PHASE_MARK && (fullGC || !markerActive);
        gcGen = computeGCGen(&gc->scavTrip, opt->block.gen, opt->gc.scav.multiplicity, initMark || finishMark || fullGC);
    } else {
        gcGen = computeGCGen(&gc->scavTrip, opt->block.gen, opt->gc.scav.multiplicity, initMark || fullGC);
        finishMark = gc->msPhase == CHI_MS_PHASE_MARK && (fullGC || gcGen == opt->block.gen - 1);
    }

    heapCheck(proc, initMark ? CHI_HEAPCHECK_PHASECHANGE : CHI_HEAPCHECK_NORMAL);

    if (initMark) {
        markerSetup(rt);
        gc->msPhase = CHI_MS_PHASE_MARK;
        CHI_EVENT0(rt, GC_MARKSWEEP_BEGIN);
        CHI_EVENT0(rt, GC_MARK_PHASE_BEGIN);
    }

    scavenger(proc, gcGen, initMark || finishMark);
    heapCheck(proc, CHI_HEAPCHECK_NORMAL);

    if (gc->msPhase != CHI_MS_PHASE_IDLE)
        markSweepSlice(proc, fullGC, finishMark);

    if (CHI_AND(CHI_HEAP_PROF_ENABLED, gcGen >= opt->heap.prof && CHI_EVENT_P(proc->rt, HEAP_PROF_BEGIN)))
        heapProf(proc);

    if (CHI_HEAP_DUMP_ENABLED && gc->dumpHeap)
        heapDump(proc);

    CHI_EVENT(proc, GC_SLICE_END, .trigger = trigger);

    if (gc->msPhase == CHI_MS_PHASE_MARK && hasMarker(gc))
        markerResume(proc, gc->conc->marker);
}

void chiGCSetup(ChiRuntime* rt) {
    ChiGC* gc = &rt->gc;
    chiMutexInit(&gc->roots.mutex);
}

void chiGCDestroy(ChiRuntime* rt) {
    ChiGC* gc = &rt->gc;
    if (hasSweeper(gc))
        sweeperDestroy(gc->conc->sweeper);
    if (hasMarker(gc))
        markerDestroy(gc->conc->marker);
    chiMutexDestroy(&gc->roots.mutex);
    chiRootVecFree(&gc->roots);
    chiPoisonMem(gc, CHI_POISON_DESTROYED, sizeof (ChiGC));
}

void chiGCProcStop(ChiProcessor* proc) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    chiBlockVecFree(&proc->gcPS.grayList);
    for (size_t gen = opt->block.gen; gen --> 0;)
        chiBlockVecFree(proc->gcPS.dirty + gen);
}

void chiGCProcStart(ChiProcessor* proc) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    chiBlockVecInit(&proc->gcPS.grayList, &proc->rt->blockTemp);
    for (size_t gen = opt->block.gen; gen --> 0;)
        chiBlockVecInit(proc->gcPS.dirty + gen, &proc->rt->tenure.manager);
}

void chiGCBlock(bool block) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiGC* gc = &proc->rt->gc;
    if (block) {
        ++gc->blocked;
        CHI_EVENT0(proc, GC_BLOCK);
    } else {
        if (--gc->blocked >= 0)
            CHI_EVENT0(proc, GC_UNBLOCK);
        else
            chiWarn("GC is already unblocked");
    }
}

void chiGCTrigger(ChiRuntime* rt, ChiGCRequestor requestor, ChiGCTrigger trigger, ChiDump dump) {
    CHI_ASSERT(trigger != CHI_GC_TRIGGER_INACTIVE);
    ChiGC* gc = &rt->gc;
    if (gc->blocked
        || rt->option.gc.mode == CHI_GC_NONE
        || (requestor == CHI_GC_REQUESTOR_HEAP && rt->option.gc.mode == CHI_GC_NOMS))
        return;
    for (;;) {
        ChiDump oldDump = gc->dumpHeap;
        if (dump <= oldDump || atomic_compare_exchange_weak(&gc->dumpHeap, &oldDump, dump))
            break;
    }
    for (;;) {
        ChiGCTrigger oldTrigger = gc->trigger;
        if (trigger <= oldTrigger)
            break;
        if (atomic_compare_exchange_weak(&gc->trigger, &oldTrigger, trigger)) {
            chiProcessorInterrupt(rt);
            CHI_EVENT(rt, GC_REQ, .requestor = requestor, .trigger = trigger);
            break;
        }
    }
}

void chiHeapSweepNotify(ChiHeap* heap) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    if (hasSweeper(&rt->gc)) {
        sweeperNotify(rt->gc.conc->sweeper);
        CHI_EVENT0(rt, GC_SWEEP_NOTIFY);
    }
}

Chili chiRoot(ChiRuntime* rt, Chili root) {
    root = chiPin(root);
    CHI_LOCK(&rt->gc.roots.mutex);
    chiRootVecPush(&rt->gc.roots, root);
    return root;
}

void chiUnroot(ChiRuntime* rt, Chili root) {
    ChiRootVec* roots = &rt->gc.roots;
    CHI_LOCK(&roots->mutex);
    for (size_t i = 0; i < roots->used; ++i) {
        if (chiIdentical(roots->elem[i], root)) {
            chiRootVecRemove(roots, i);
            break;
        }
    }
}
