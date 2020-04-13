#include "barrier.h"
#include "error.h"
#include "event.h"
#include "new.h"
#include "sink.h"
#include "timeout.h"
#include "tracepoint.h"

static void markSlice(ChiWorker* worker, ChiGrayVec* gray, ChiMarkState markState, ChiTimeout* timeout) {
    chiEvent0(worker, GC_MARK_SLICE_BEGIN);

    ChiScanStats stats;
    const ChiRuntimeOption* opt = &worker->rt->option;
    chiMarkSlice(gray, opt->heap.scanDepth, !opt->gc.major.noCollapse, markState, &stats, timeout);

    CHI_TTW(worker, .name = "runtime;gc;mark");
    chiEventStruct(worker, GC_MARK_SLICE_END, &stats);
}

static void sweepSlice(ChiWorker* worker, ChiMarkState markState, ChiTimeout* timeout) {
    chiEvent0(worker, GC_SWEEP_SLICE_BEGIN);
    ChiSweepStats stats;
    chiSweepSlice(&worker->rt->heap, &worker->rt->gc, markState, &stats, timeout);
    chiEventStruct(worker, GC_SWEEP_SLICE_END, &stats);
    CHI_TTW(worker, .name = "runtime;gc;sweep");
}

static void gcSlice(ChiWorker* worker, ChiGrayVec* gray, ChiMarkState markState, ChiMicros slice) {
    ChiTimeout timeout;
    chiTimeoutStart(&timeout, slice);
    if (chiTriggered(&worker->rt->gc.sweep.needed))
        sweepSlice(worker, markState, &timeout);
    if (chiTimeoutTick(&timeout) && atomic_load_explicit(&worker->rt->gc.phase, memory_order_relaxed) == CHI_GC_ASYNC && !chiGrayNull(gray))
        markSlice(worker, gray, markState, &timeout);
    chiTimeoutStop(&timeout);
}

#if CHI_GC_CONC_ENABLED
typedef enum {
    WORKER_WAIT,
    WORKER_EPOCH,
    WORKER_RUN,
    WORKER_STOP,
} GCWorkerStatus;

struct ChiGCWorker_ {
    ChiRuntime*    rt;
    ChiTask        task;
    ChiCond        cond;
    ChiTimeout     timeout;
    ChiGrayVec     gray;
    GCWorkerStatus status;
    ChiMarkState  markState;
};

static bool gcWorkerSync(ChiGCWorker* w) {
    ChiGC* gc = &w->rt->gc;
    CHI_LOCK_MUTEX(&gc->mutex);
    for (;;) {
        while (w->status == WORKER_WAIT)
            chiCondWait(&w->cond, &gc->mutex);

        if (w->status == WORKER_STOP)
            return false;

        if (w->status == WORKER_EPOCH) {
            chiMarkStateEpoch(&w->markState);
            w->status = WORKER_RUN;
        }

        if (chiTriggered(&gc->sweep.needed))
            return true;

        if (!chiGrayNull(&gc->gray.vec)) {
            chiGrayTake(&w->gray, &gc->gray.vec);
            return true;
        }

        w->status = WORKER_WAIT;
    }
}

CHI_TASK(gcWorkerRun, arg) {
    ChiWorker worker = {};
    ChiGCWorker* w = (ChiGCWorker*)arg;
    chiWorkerBegin(&worker, w->rt, "gc");
    while (gcWorkerSync(w)) {
        CHI_TTW(&worker, .name = "runtime;suspend");
        gcSlice(&worker, &w->gray, w->markState, CHI_TIMEOUT_INFINITE);
    }
    chiWorkerEnd(&worker);
    return 0;
}

static void gcWorkerStart(ChiRuntime* rt, ChiMarkState markState) {
    ChiGC* gc = &rt->gc;
    CHI_LOCK_MUTEX(&gc->mutex);
    uint32_t numWorker = rt->option.gc.major.worker;
    if (gc->firstWorker || !numWorker)
        return;
    gc->firstWorker = gc->lastWorker = chiZallocArr(ChiGCWorker, numWorker);
    for (ChiGCWorker* w = gc->firstWorker; w < gc->firstWorker + numWorker; ++w) {
        w->rt = rt;
        chiCondInit(&w->cond);
        chiGrayInit(&w->gray, &gc->gray.manager);
        w->markState = markState;
        w->task = chiTaskTryCreate(gcWorkerRun, w);
        if (chiTaskNull(w->task))
            break;
        ++gc->lastWorker;
    }
}

static void gcWorkerStop(ChiGC* gc) {
    CHI_LOCK_MUTEX(&gc->mutex);
    for (ChiGCWorker* w = gc->firstWorker; w < gc->lastWorker; ++w) {
        w->status = WORKER_STOP;
        chiCondSignal(&w->cond);
        chiTimeoutTrigger(&w->timeout, true);
    }
}

static void gcWorkerDestroy(ChiRuntime* rt) {
    ChiGC* gc = &rt->gc;
    gcWorkerStop(gc);
    for (ChiGCWorker* w = gc->firstWorker; w < gc->lastWorker; ++w) {
        chiTaskJoin(w->task);
        chiCondDestroy(&w->cond);
        chiGrayFree(&w->gray);
    }
    chiFree(gc->firstWorker);
}

static void gcWorkerNotify(ChiGC* gc, GCWorkerStatus status) {
    for (ChiGCWorker* w = gc->firstWorker; w < gc->lastWorker; ++w) {
        if (w->status == WORKER_WAIT) {
            w->status = status;
            chiCondSignal(&w->cond);
        }
    }
}

static void gcWorkerEpochSync(ChiRuntime* rt) {
    {
        ChiGC* gc = &rt->gc;
        CHI_LOCK_MUTEX(&gc->mutex);
        gcWorkerNotify(gc, WORKER_EPOCH);
    }
    chiEvent0(rt, GC_NOTIFY);
}

static bool gcWorkerEpochAck(ChiRuntime* rt) {
    ChiGC* gc = &rt->gc;
    CHI_LOCK_MUTEX(&gc->mutex);
    for (ChiGCWorker* w = gc->firstWorker; w < gc->lastWorker; ++w) {
        if (w->status == WORKER_EPOCH)
            return false;
    }
    return true;
}

static bool gcWorkerGrayNull(ChiRuntime* rt) {
    ChiGC* gc = &rt->gc;
    CHI_LOCK_MUTEX(&gc->mutex);
    for (ChiGCWorker* w = gc->firstWorker; w < gc->lastWorker; ++w) {
        if (w->status != WORKER_WAIT || !chiGrayNull(&w->gray))
            return false;
    }
    return true;
}

static void gcWorkerGrayPush(ChiRuntime* rt, ChiGrayVec* gray) {
    if (chiGrayNull(gray) || rt->option.gc.major.mode != CHI_GC_CONC)
        return;
    {
        ChiGC* gc = &rt->gc;
        CHI_LOCK_MUTEX(&gc->mutex);
        chiGrayJoin(&gc->gray.vec, gray);
        gcWorkerNotify(gc, WORKER_RUN);
    }
    chiEvent0(rt, GC_NOTIFY);
}
#else
static void gcWorkerStart(ChiRuntime* CHI_UNUSED(rt), ChiMarkState CHI_UNUSED(markState)) {}
static void gcWorkerDestroy(ChiRuntime* CHI_UNUSED(rt)) {}
static void gcWorkerEpochSync(ChiRuntime* CHI_UNUSED(rt)) {}
static bool gcWorkerEpochAck(ChiRuntime* CHI_UNUSED(rt)) { return true; }
static void gcWorkerGrayPush(ChiRuntime* CHI_UNUSED(rt), ChiGrayVec* CHI_UNUSED(gray)) {}
static bool gcWorkerGrayNull(ChiRuntime* CHI_UNUSED(rt)) { return true; }
#endif

void chiGCSetup(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;
    ChiGC* gc = &rt->gc;
    chiMutexInit(&gc->mutex);
    chiBlockManagerSetup(&gc->gray.manager, opt->block.size, opt->block.chunk, true, rt);
    chiGrayInit(&gc->gray.vec, &gc->gray.manager);
    chiChunkListInit(&gc->sweep.largeList);
    chiHeapSegmentListInit(&gc->sweep.segmentUnswept);
    chiHeapSegmentListInit(&gc->sweep.segmentPartial);
}

void chiGCDestroy(ChiRuntime* rt) {
    ChiGC* gc = &rt->gc;
    gcWorkerDestroy(rt);
    chiMutexDestroy(&gc->mutex);
    chiGrayFree(&gc->gray.vec);
    chiBlockManagerDestroy(&gc->gray.manager);
    chiVecFree(&gc->root);
    chiHeapSegmentListFree(&rt->heap, &gc->sweep.segmentPartial);
    chiHeapSegmentListFree(&rt->heap, &gc->sweep.segmentUnswept);
    chiHeapChunkListFree(&rt->heap, &gc->sweep.largeList);
    CHI_POISON_STRUCT(gc, CHI_POISON_DESTROYED);
}

void chiGCTrigger(ChiRuntime* rt) {
    ChiGC* gc = &rt->gc;
    if (rt->option.gc.major.mode == CHI_GC_NONE)
        return;
    if (!chiTriggerExchange(&gc->trigger, true)) {
        chiEvent0(rt, GC_TRIGGER);
        chiProcessorRequestMain(rt, CHI_REQUEST_HANDSHAKE);
    }
}

void chiGCRoot(ChiRuntime* rt, Chili root) {
    CHI_ASSERT(chiGen(root) == CHI_GEN_MAJOR);
    ChiGC* gc = &rt->gc;
    CHI_LOCK_MUTEX(&gc->mutex);
    chiVecAppend(&gc->root, root);
}

void chiGCUnroot(ChiRuntime* rt, Chili root) {
    ChiGC* gc = &rt->gc;
    CHI_LOCK_MUTEX(&gc->mutex);
    for (size_t i = 0; i < gc->root.used; ++i) {
        if (chiIdentical(gc->root.elem[i], root)) {
            chiVecDelete(&gc->root, i);
            break;
        }
    }
}

static void gcLocalPhase(ChiProcessor* proc, ChiGCPhase phase) {
    CHI_ASSERT(atomic_load_explicit(&proc->gc.phase, memory_order_relaxed) != phase);
    atomic_store_explicit(&proc->gc.phase, phase, memory_order_relaxed);
    chiEvent(proc, GC_PHASE_LOCAL, .phase = phase);
}

static void markRoots(ChiProcessor* proc) {
    ChiLocalGC* lgc = &proc->gc;
    // Global roots
    if (chiProcessorMain(proc)) {
        ChiGC* gc = &proc->rt->gc;
        CHI_LOCK_MUTEX(&gc->mutex);
        for (size_t i = 0; i < gc->root.used; ++i)
            chiGrayMark(lgc, gc->root.elem[i]);
    }
    // Local roots
    chiGrayMarkUnboxed(lgc, proc->local); // unboxed, since it might be uninitialized
    chiGrayMark(lgc, proc->thread);
}

static bool gcPoll(ChiProcessor* proc, ChiGCPhase phase) {
    ChiRuntime* rt = proc->rt;
    CHI_ASSERT(atomic_load_explicit(&rt->gc.phase, memory_order_relaxed) == phase);
    bool done = true;
    {
        CHI_LOCK_MUTEX(&rt->proc.mutex);
        CHI_FOREACH_PROCESSOR (p, rt) {
            if (atomic_load_explicit(&p->gc.phase, memory_order_relaxed) != phase) {
                done = false;
                if (p != proc)
                    chiProcessorRequest(p, CHI_REQUEST_HANDSHAKE);
            }
        }
    }
    return done;
}

static void gcGlobalPhase(ChiProcessor* proc, ChiGCPhase phase) {
    CHI_ASSERT(atomic_load_explicit(&proc->rt->gc.phase, memory_order_relaxed) != phase);
    atomic_store_explicit(&proc->rt->gc.phase, phase, memory_order_relaxed);
    chiEvent(proc, GC_PHASE_GLOBAL, .phase = phase);
}

static bool gcGlobalTransition(ChiProcessor* proc, ChiGCPhase from, ChiGCPhase to) {
    if (gcPoll(proc, from)) {
        gcGlobalPhase(proc, to);
        return true;
    }
    return false;
}

static bool markDone(ChiRuntime* rt) {
    {
        CHI_LOCK_MUTEX(&rt->proc.mutex);
        CHI_FOREACH_PROCESSOR (p, rt) {
            if (!chiGrayNull(&p->gc.gray))
                return false;
        }
    }
    return gcWorkerGrayNull(rt) && chiGrayNull(&rt->gc.gray.vec);
}

static void gcControl(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    ChiGC* gc = &rt->gc;

    if (chiEventEnabled(proc, HEAP_USAGE)) {
        ChiHeapUsage usage = { .totalChunkWords = atomic_load_explicit(&proc->rt->totalHeapChunkSize,
                                                                       memory_order_relaxed) / CHI_WORDSIZE };
        chiHeapUsage(&rt->heap, &usage);
        chiEventStruct(proc, HEAP_USAGE, &usage);
    }

    switch (atomic_load_explicit(&gc->phase, memory_order_relaxed)) {
    case CHI_GC_IDLE:
        if (chiTriggered(&gc->trigger) && gcWorkerEpochAck(rt))
            gcGlobalTransition(proc, CHI_GC_IDLE, CHI_GC_SYNC1);
        break;
    case CHI_GC_SYNC1:
        gcGlobalTransition(proc, CHI_GC_SYNC1, CHI_GC_SYNC2);
        break;
    case CHI_GC_SYNC2:
        if (gcGlobalTransition(proc, CHI_GC_SYNC2, CHI_GC_ASYNC)) {
            chiEvent0(rt, GC_MARK_PHASE_BEGIN);
            gcWorkerStart(rt, proc->gc.markState);
        }
        break;
    case CHI_GC_ASYNC:
        if (markDone(rt) && gcGlobalTransition(proc, CHI_GC_ASYNC, CHI_GC_IDLE)) {
            chiEvent0(rt, GC_MARK_PHASE_END);
            {
                CHI_LOCK_MUTEX(&gc->mutex);
                chiHeapSweepAcquire(&rt->heap, &gc->sweep.largeList, &gc->sweep.segmentUnswept);
                chiTrigger(&gc->sweep.needed, true);
            }
            chiTrigger(&gc->trigger, false);
            gcWorkerEpochSync(rt);
        }
        break;
    }
}

static void minorStats(ChiProcessor* proc, ChiMinorHeapUsage* stats) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    ChiBlockManager* bm = &proc->heap.manager;
    stats->usedWords = opt->block.size * (bm->blocksCount - bm->freeList.length) / CHI_WORDSIZE;
    stats->totalWords = opt->block.size * bm->blocksCount / CHI_WORDSIZE;
}

static void scavenger(ChiProcessor* proc, uint32_t aging, bool snapshot) {
    ChiEventScavenger event = {};
#if CHI_COUNT_ENABLED
    event.promoted = proc->heap.promoted.stats;
    CHI_ZERO_STRUCT(&proc->heap.promoted.stats);
#endif

    minorStats(proc, &event.minorHeapBefore);
    chiEvent0(proc, GC_SCAVENGER_BEGIN);
    chiScavenger(proc, aging, snapshot, &event.scavenger);
    minorStats(proc, &event.minorHeapAfter);
    chiEventStruct(proc, GC_SCAVENGER_END, &event);
}

static bool gcLocalTransition(ChiProcessor* proc, ChiGCPhase from, ChiGCPhase to) {
    ChiGCPhase phase = atomic_load_explicit(&proc->gc.phase, memory_order_relaxed);
    CHI_ASSERT(phase == from || phase == to);
    if (phase == from) {
        gcLocalPhase(proc, to);
        return true;
    }
    return false;
}

static void gcIncSlice(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    ChiLocalGC* lgc = &proc->gc;
    if (chiProcessorMain(proc)) {
        // Take over global gray list. It might be non-empty due to processor shutdown.
        ChiGC* gc = &rt->gc;
        CHI_LOCK_MUTEX(&gc->mutex);
        chiGrayJoin(&lgc->gray, &gc->gray.vec);
    }
    gcSlice(proc->worker, &lgc->gray, lgc->markState, rt->option.gc.major.slice);
}

static void gcHandshake(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    ChiLocalGC* gc = &proc->gc;
    bool snapshot = false;

    switch (atomic_load_explicit(&rt->gc.phase, memory_order_relaxed)) {
    case CHI_GC_SYNC1:
        if (gcLocalTransition(proc, CHI_GC_IDLE, CHI_GC_SYNC1))
            gc->markState.barrier = _CHI_BARRIER_SYNC;
        break;
    case CHI_GC_SYNC2:
        gcLocalTransition(proc, CHI_GC_SYNC1, CHI_GC_SYNC2);
        break;
    case CHI_GC_ASYNC:
        if (atomic_load_explicit(&gc->phase, memory_order_relaxed) == CHI_GC_SYNC2) {
            snapshot = true;
            proc->handle.allocColor = gc->markState.black;
            gc->markState.barrier = _CHI_BARRIER_ASYNC;
            markRoots(proc);
        } else {
            CHI_ASSERT(atomic_load_explicit(&gc->phase, memory_order_relaxed) == CHI_GC_ASYNC);
        }
        break;
    case CHI_GC_IDLE:
        if (gcLocalTransition(proc, CHI_GC_ASYNC, CHI_GC_IDLE)) {
            CHI_ASSERT(chiGrayNull(&gc->gray));
            gc->markState.barrier = _CHI_BARRIER_NONE;
            chiMarkStateEpoch(&gc->markState);
            CHI_IF(CHI_POISON_ENABLED, _chiDebugData.markState = gc->markState;)
        }
        break;
    }

    if (chiTriggered(&proc->trigger.scavenge) || snapshot) {
        uint32_t aging = rt->option.gc.scav.aging;
        if (chiTriggered(&proc->trigger.migrate)) {
            chiTrigger(&proc->trigger.migrate, false);
            aging = 1; // promote everything to major heap
        }

        scavenger(proc, aging, snapshot);
        chiTrigger(&proc->trigger.scavenge, false);

        if (snapshot)
            gcLocalPhase(proc, CHI_GC_ASYNC);

        if (rt->option.gc.major.mode == CHI_GC_INC)
            gcIncSlice(proc);
    }

    gcWorkerGrayPush(rt, &gc->gray);
}

void chiGCService(ChiProcessor* proc) {
    if (chiProcessorMain(proc))
        gcControl(proc);
    gcHandshake(proc);
}

void chiLocalGCSetup(ChiLocalGC* lgc, ChiGC* gc) {
    chiGrayInit(&lgc->gray, &gc->gray.manager);
    chiMarkStateInit(&lgc->markState);
}

void chiLocalGCDestroy(ChiLocalGC* lgc, ChiGC* gc) {
    {
        CHI_LOCK_MUTEX(&gc->mutex);
        chiGrayJoin(&gc->gray.vec, &lgc->gray);
    }
    CHI_POISON_STRUCT(lgc, CHI_POISON_DESTROYED);
}

void chiGrayMarkBarrier(ChiLocalGC* gc, Chili old, Chili val) {
    CHI_ASSERT(gc->markState.barrier);
    chiGrayMarkUnboxed(gc, old);
    if (gc->markState.barrier == _CHI_BARRIER_SYNC)
        chiGrayMarkUnboxed(gc, val);
}
