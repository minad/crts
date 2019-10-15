#include <stdlib.h>
#include "error.h"
#include "event.h"
#include "export.h"
#include "new.h"
#include "profiler.h"
#include "runtime.h"
#include "sink.h"
#include "stack.h"

#define VEC_NOSTRUCT
#define VEC ChiHookVec
#define VEC_TYPE ChiHook
#define VEC_PREFIX hookVec
#include "generic/vec.h"

typedef struct {
    size_t hard, soft;
} HeapLimits;

CHI_INL HeapLimits heapLimits(const ChiRuntimeOption* opt) {
    return (HeapLimits) {
        .hard = (size_t)(((uint64_t)opt->heap.limit.hard * opt->heap.limit.max) / 100),
        .soft = (size_t)(((uint64_t)opt->heap.limit.soft * opt->heap.limit.max) / 100)
    };
}

static bool heapLimitReached(ChiRuntime* rt, size_t size) {
    return atomic_load_explicit(&rt->totalHeapChunkSize, memory_order_relaxed) + size > heapLimits(&rt->option).hard;
}

static size_t heapGrow(ChiRuntime* rt, size_t size) {
    size_t heapSize = atomic_fetch_add_explicit(&rt->totalHeapChunkSize, size, memory_order_relaxed) + size;
    HeapLimits limits = heapLimits(&rt->option);
    if (heapSize > limits.soft || heapSize > limits.hard) {
        if (heapSize > limits.hard)
            chiTrigger(&rt->heapOverflow, true);
        chiEvent(rt, HEAP_LIMIT,
                 .heapSize = heapSize,
                 .softLimit = limits.soft,
                 .hardLimit = limits.hard,
                 .limit = heapSize > limits.hard ? CHI_HEAP_LIMIT_HARD : CHI_HEAP_LIMIT_SOFT);
        chiGCTrigger(rt);
    }
    return heapSize;
}

static size_t heapShrink(ChiRuntime* rt, size_t size) {
    return atomic_fetch_sub_explicit(&rt->totalHeapChunkSize, size, memory_order_relaxed) - size;
}

void chiBlockAllocLimitReached(ChiBlockManager* bm) {
    ChiProcessor* proc = CHI_OUTER(ChiProcessor, heap, CHI_OUTER(ChiLocalHeap, manager, bm));
    chiProcessorRequest(proc, CHI_REQUEST_SCAVENGE);
}

ChiChunk* chiBlockManagerChunkNew(ChiRuntime* rt, size_t size, size_t align) {
    ChiChunk* chunk = chiChunkNew(size, align);
    /* Block allocations must never fail.
     * If we cannot allocate the fairly small chunks used by
     * the block manager, the system must be under immense pressure
     * and termination is the only reasonable thing to do.
     */
    if (!chunk)
        chiErr("Block manager failed to allocate chunk: size=%Z align=%Z heapSize=%Z: %m",
               size, align, atomic_load_explicit(&rt->totalHeapChunkSize, memory_order_relaxed));
    size_t heapSize = heapGrow(rt, size);
    chiEvent(rt, BLOCK_CHUNK_NEW, .size = size, .align = align,
             .start = (uintptr_t)chunk->start, .heapSize = heapSize);
    return chunk;
}

void chiBlockManagerChunkFree(ChiRuntime* rt, ChiChunk* chunk) {
    size_t heapSize = heapShrink(rt, chunk->size);
    chiEvent(rt, BLOCK_CHUNK_FREE, .size = chunk->size, .start = (uintptr_t)chunk->start,
             .heapSize = heapSize);
    chiChunkFree(chunk);
}

ChiChunk* chiHeapChunkNew(ChiHeap* heap, size_t size, ChiNewFlags flags) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    if ((flags & CHI_NEW_TRY) && heapLimitReached(rt, size))
        return 0;
    ChiChunk* chunk = chiChunkNew(size, 0);
    if (!chunk) {
        if ((flags & CHI_NEW_TRY))
            return 0;
        chiErr("Heap failed to allocate chunk: size=%Z totalHeapChunkSize=%Z: %m",
               size, atomic_load_explicit(&rt->totalHeapChunkSize, memory_order_relaxed));
    }
    size_t heapSize = heapGrow(rt, size);
    chiEvent(rt, HEAP_CHUNK_NEW,
              .size = size, .start = (uintptr_t)chunk->start, .heapSize = heapSize);
    return chunk;
}

void chiHeapChunkFree(ChiHeap* heap, ChiChunk* chunk) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    size_t heapSize = heapShrink(rt, chunk->size);
    chiEvent(rt, HEAP_CHUNK_FREE, .size = chunk->size, .start = (uintptr_t)chunk->start,
              .heapSize = heapSize);
    chiChunkFree(chunk);
}

void chiHeapLimitReached(ChiHeap* heap) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    HeapLimits limits = heapLimits(&rt->option);
    chiEvent(rt, HEAP_LIMIT,
             .heapSize = atomic_load_explicit(&rt->totalHeapChunkSize, memory_order_relaxed),
             .softLimit = limits.soft,
             .hardLimit = limits.hard,
             .limit = CHI_HEAP_LIMIT_ALLOC);
    chiGCTrigger(rt);
}

#if CHI_SYSTEM_HAS_INTERRUPT
static ChiMicros timerHandler(ChiTimer* timer) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, interrupts, CHI_OUTER(ChiInterrupts, timer, timer));
    chiEvent0(rt, TICK);
#if CHI_SYSTEM_HAS_STATS
    if (chiEventEnabled(rt, SYSTEM_STATS)) {
        ChiSystemStats stats;
        chiSystemStats(&stats);
        chiEventStruct(rt, SYSTEM_STATS, &stats);
    }
#endif
    chiProcessorRequestAll(rt, CHI_REQUEST_TICK);
    return timer->timeout;
}

static void signalHandler(ChiSigHandler* handler, ChiSig sig) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, interrupts, CHI_OUTER(ChiInterrupts, sigHandler, handler));
    chiEvent(rt, SIGNAL, .sig = sig);

    switch (sig) {
    case CHI_SIG_USERINTERRUPT:
        chiProcessorRequestMain(rt, CHI_REQUEST_USERINTERRUPT);
        break;

    case CHI_SIG_DUMPSTACK:
        chiProcessorRequestAll(rt, CHI_REQUEST_DUMPSTACK);
        break;
    }
}

static void setupInterrupt(ChiInterrupts* is, ChiMillis interval) {
    is->sigHandler.handler = signalHandler;
    chiSigInstall(&is->sigHandler);
    is->timer.handler = timerHandler;
    is->timer.timeout = chiMillisToMicros(interval);
    chiTimerInstall(&is->timer);
}

static void destroyInterrupt(ChiInterrupts* is) {
    chiSigRemove(&is->sigHandler);
    chiTimerRemove(&is->timer);
}
#endif

static void hookFree(ChiRuntime* rt) {
    for (size_t i = 0; i < CHI_DIM(rt->hooks); ++i)
        hookVecFree(rt->hooks + i);
}

void chiHook(ChiRuntime* rt, ChiHookType type, ChiHookFn fn) {
    hookVecAppend(rt->hooks + type, (ChiHook){ .type = type, .fn = fn });
}

void chiHookRun(ChiHookType type, void* ctx) {
    const ChiHookVec* v = (type <= CHI_HOOK_PROC ? ((ChiProcessor*)ctx)->rt : ((ChiWorker*)ctx)->rt)->hooks + type;
    if (type & 1) {
        for (size_t i = v->used; i --> 0;)
            CHI_VEC_AT(v, i).fn(type, ctx);
    } else {
        for (size_t i = 0; i < v->used; ++i)
            CHI_VEC_AT(v, i).fn(type, ctx);
    }
}

static void setupStack(ChiProcessor* proc) {
    proc->thread = chiThreadNewUninitialized(proc);
    ChiStack* stack = chiToStack(chiThreadStackInactive(proc->thread));
    *stack->sp++ = chiFromCont(&chiRunMainCont);
    *stack->sp++ = chiFromCont(&z_Main);
    *stack->sp++ = chiFromCont(&chiJumpCont);
    *stack->sp++ = chiFromCont(&chiStartupEndCont);
    *stack->sp++ = chiFromCont(&chiSetupEntryPointsCont);
    *stack->sp++ = chiFromCont(&z_System__EntryPoints);
    *stack->sp++ = chiFromCont(&chiJumpCont);
}

static ChiProcessor* setup(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;
    CHI_IF(CHI_STATS_ENABLED, chiStatsSetup(&rt->stats, opt->stats.tableCell, opt->stats.json));
    chiEventSetup(rt);
    ChiProcessor* proc = chiProcessorBegin(rt);
    chiHeapSetup(&rt->heap,
                 opt->heap.small.segment, opt->heap.small.page,
                 opt->heap.medium.segment, opt->heap.medium.page,
                 opt->heap.limit.full, opt->heap.limit.init);
    chiGCSetup(rt);
    chiProcessorSetup(proc);
    rt->entryPoint.unhandled = chiNewVariadicFlags(proc, CHI_FN(2), CHI_NEW_SHARED | CHI_NEW_CLEAN,
                                                   chiFromCont(&chiEntryPointUnhandledDefault));
    chiGCRoot(rt, rt->entryPoint.unhandled);
    setupStack(proc);
    CHI_IF(CHI_SYSTEM_HAS_INTERRUPT, setupInterrupt(&rt->interrupts, opt->interval));
    return proc;
}

void chiRuntimeDestroy(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    CHI_ASSERT(chiProcessorMain(proc));
    hookFree(rt);
    CHI_IF(CHI_SYSTEM_HAS_INTERRUPT, destroyInterrupt(&rt->interrupts));
    chiHeapDestroy(&rt->heap);
    chiFree(rt->moduleHash.entry);
    chiEventDestroy(rt);
    CHI_IF(CHI_STATS_ENABLED, chiStatsDestroy(&rt->stats, rt->option.stats.file,
                                              CHI_AND(CHI_COLOR_ENABLED, rt->option.color)));
    chiMutexDestroy(&rt->proc.mutex);
    chiCondDestroy(&rt->proc.cond);
    proc->rt = proc->worker->rt = 0;
    rt->exitHandler(rt->exitCode);
}

void chiRuntimeEnter(ChiRuntime* rt, int argc, char** argv) {
    ChiRuntimeOption* opt = &rt->option;
    chiRuntimeOptionValidate(opt);

    CHI_ASSERT(argc >= 0);
    rt->argc = (uint32_t)argc;
    rt->argv = argv;

    chiProcessorEnter(setup(rt));
}

uint64_t chiCurrentTimeNanos(void) {
    return CHI_UN(Nanos, chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), CHI_CURRENT_RUNTIME->timeRef.start.real));
}

uint64_t chiCurrentTimeMillis(void) {
    return CHI_UN(Millis, chiNanosToMillis(chiNanosDelta(chiClock(CHI_CLOCK_REAL_FAST), CHI_CURRENT_RUNTIME->timeRef.start.real)));
}

void chiRuntimeInit(ChiRuntime* rt, ChiExitHandler exitHandler) {
    CHI_ZERO_STRUCT(rt);
    chiRuntimeOptionDefaults(&rt->option);
    rt->timeRef.start = chiTime();
    rt->exitHandler = exitHandler;
}

static ChiOptionResult parseOptions(ChiRuntimeOption* opt, ChiChunkOption* chunkOption, ChiProfOption* profOption,
                         int* argc, char** argv) {
    CHI_NOWARN_UNUSED(profOption);
    CHI_NOWARN_UNUSED(chunkOption);
    CHI_AUTO_SINK(helpSink, chiSinkColor(CHI_SINK_COLOR_AUTO));
    const ChiOptionList list[] =
        { { chiRuntimeOptionList, opt },
          CHI_IF(CHI_SYSTEM_HAS_MALLOC, { chiChunkOptionList, chunkOption },)
          CHI_IF(CHI_PROF_ENABLED, { chiProfOptionList, profOption },)
          { 0, 0 }
        };
    const ChiOptionParser parser =
        { .help = helpSink,
          .usage = chiStderr,
          .list = list
        };
    ChiOptionResult res = chiOptionEnv(&parser, "CHI_NATIVE_OPTS");
    if (res == CHI_OPTRESULT_OK)
        res = chiOptionArgs(&parser, argc, argv);
    if (res == CHI_OPTRESULT_HELP) {
        chiPager();
        chiOptionHelp(&parser);
    }
    return res;
}

// We allow to use more stack space here to store
// the rather large ChiRuntime struct.
_Noreturn void chiRuntimeMain(ChiRuntime* rt, int argc, char** argv, ChiExitHandler exitHandler) {
    ChiChunkOption chunkOption = CHI_DEFAULT_CHUNK_OPTION;
    ChiProfOption profOption = CHI_DEFAULT_PROF_OPTION;
    chiRuntimeInit(rt, exitHandler);

    ChiOptionResult res = parseOptions(&rt->option, &chunkOption, &profOption, &argc, argv);
    if (res != CHI_OPTRESULT_OK)
        exitHandler(res == CHI_OPTRESULT_ERROR ? CHI_EXIT_ERROR : CHI_EXIT_SUCCESS);

    if (CHI_SYSTEM_HAS_MALLOC)
        chiChunkSetup(&chunkOption);

    if (CHI_PROF_ENABLED && (profOption.file[0] || profOption.flat || profOption.alloc))
        chiProfilerSetup(rt, &profOption);

    chiRuntimeEnter(rt, argc, argv);
}
