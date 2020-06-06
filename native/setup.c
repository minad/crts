#include <stdlib.h>
#include "error.h"
#include "event.h"
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

static size_t heapGrow(ChiRuntime* rt, size_t size) {
    size_t heapSize = atomic_fetch_add_explicit(&rt->heapTotalSize, size, memory_order_relaxed) + size;
    if (heapSize > rt->heapOverflowLimit) {
        chiTrigger(&rt->heapOverflow, true);
        chiEvent0(rt, HEAP_LIMIT_OVERFLOW);
        chiGCTrigger(rt);
    }
    return heapSize;
}

static size_t heapShrink(ChiRuntime* rt, size_t size) {
    return atomic_fetch_sub_explicit(&rt->heapTotalSize, size, memory_order_relaxed) - size;
}

void chiBlockManagerAllocHook(ChiBlockManager* bm) {
    ChiLocalHeap* heap = CHI_OUTER(ChiLocalHeap, manager, bm);
    if (heap->nursery.limit <
        heap->nursery.alloc.usedList.length +
        heap->dirty.objects.list.length +
        heap->dirty.cards.list.length +
        heap->promoted.objects.list.length)
        chiProcessorRequest(CHI_OUTER(ChiProcessor, heap, heap), CHI_REQUEST_SCAVENGE);
}

ChiChunk* chiBlockManagerChunkNew(ChiBlockManager* bm, size_t size, size_t align) {
    ChiChunk* chunk = chiChunkNew(size, align);
    /* Block allocations must never fail.
     * If we cannot allocate the fairly small chunks used by
     * the block manager, the system must be under immense pressure
     * and termination is the only reasonable thing to do.
     */
    if (!chunk)
        chiErr("Block manager failed to allocate chunk: size=%Z align=%Z heapSize=%Z: %m",
               size, align, atomic_load_explicit(&bm->rt->heapTotalSize, memory_order_relaxed));
    size_t heapSize = heapGrow(bm->rt, size);
    chiEvent(bm->rt, BLOCK_CHUNK_NEW, .size = size, .align = align,
             .start = (uintptr_t)chunk->start, .heapSize = heapSize);
    return chunk;
}

void chiBlockManagerChunkFree(ChiBlockManager* bm, ChiChunk* chunk) {
    size_t heapSize = heapShrink(bm->rt, chunk->size);
    chiEvent(bm->rt, BLOCK_CHUNK_FREE, .size = chunk->size, .start = (uintptr_t)chunk->start,
             .heapSize = heapSize);
    chiChunkFree(chunk);
}

ChiChunk* chiHeapChunkNew(ChiHeap* heap, size_t size, ChiNewFlags flags) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    if ((flags & CHI_NEW_TRY) &&
        atomic_load_explicit(&rt->heapTotalSize, memory_order_relaxed) + size > rt->heapOverflowLimit)
        return 0;
    ChiChunk* chunk = chiChunkNew(size, 0);
    if (!chunk) {
        if ((flags & CHI_NEW_TRY))
            return 0;
        chiErr("Heap failed to allocate chunk: chunkSize=%Z heapTotalSize=%Z: %m",
               size, atomic_load_explicit(&rt->heapTotalSize, memory_order_relaxed));
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
    chiEvent0(rt, HEAP_LIMIT_GC);
    chiGCTrigger(rt);
}

#if CHI_SYSTEM_HAS_INTERRUPT
static ChiMicros timerHandler(ChiTimer* timer) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, interrupts, CHI_OUTER(ChiInterrupts, timer, timer));
#if CHI_SYSTEM_HAS_STATS
    if (chiEventEnabled(rt, SYSTEM_STATS)) {
        ChiSystemStats ss;
        chiSystemStats(&ss);
        chiEventStruct(rt, SYSTEM_STATS, &ss);
    }
#endif
    chiProcessorRequestAll(rt, CHI_REQUEST_TIMERINTERRUPT);
    return timer->timeout;
}

static void signalHandler(ChiSigHandler* handler, ChiSig sig) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, interrupts, CHI_OUTER(ChiInterrupts, sigHandler, handler));
    chiEvent(rt, SIGNAL, .sig = sig);

    switch (sig) {
    case CHI_SIG_USERINTERRUPT:
        chiProcessorRequestMain(rt, CHI_REQUEST_USERINTERRUPT);
        break;

    case CHI_SIG_DUMP:
        chiProcessorRequestAll(rt, CHI_REQUEST_DUMP);
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

void _chiHook(ChiRuntime* rt, ChiHookType type, ChiHookFn(void) fn) {
    hookVecAppend(rt->hooks + type, (ChiHook){ .type = type, .fn = fn });
}

void _chiHookRun(ChiRuntime* rt, ChiHookType type, void* ctx) {
    const ChiHookVec* v = rt->hooks + type;
    if (type & 1) {
        for (size_t i = v->used; i --> 0;)
            CHI_VEC_AT(v, i).fn(type, ctx);
    } else {
        for (size_t i = 0; i < v->used; ++i)
            CHI_VEC_AT(v, i).fn(type, ctx);
    }
}

CHI_EXTERN_CONT_DECL(z_Main)
CHI_EXTERN_CONT_DECL(z_System__EntryPoints)

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
    rt->heapOverflowLimit = (size_t)(((uint64_t)opt->heap.limit.overflow * opt->heap.limit.max) / 100);
    chiHeapSetup(&rt->heap,
                 opt->heap.small.segment, opt->heap.small.page,
                 opt->heap.medium.segment, opt->heap.medium.page,
                 opt->heap.limit.full, opt->heap.limit.init);
    chiGCSetup(rt);
    chiProcessorSetup(proc);
    rt->fail = chiNewInl(proc, CHI_FAIL, 1, CHI_NEW_SHARED | CHI_NEW_CLEAN);
    *(ChiWord*)chiRawPayload(rt->fail) = 0;
    chiGCRoot(rt, rt->fail);
    setupStack(proc);
    CHI_IF(CHI_SYSTEM_HAS_INTERRUPT, setupInterrupt(&rt->interrupts, opt->interval));
    chiGCRoot(rt, rt->fail);
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
    chiRuntimeOptionValidate(&rt->option);
    CHI_ASSERT(argc >= 0);
    rt->argc = (uint32_t)argc;
    rt->argv = argv;
    chiProcessorEnter(setup(rt));
}

uint64_t chiCurrentTimeNanos(void) {
    return CHI_UN(Nanos, chiNanosDelta(chiClockFine(), CHI_CURRENT_RUNTIME->timeRef.start));
}

uint64_t chiCurrentTimeMillis(void) {
    return CHI_UN(Millis, chiNanosToMillis(chiNanosDelta(chiClockFast(), CHI_CURRENT_RUNTIME->timeRef.start)));
}

void chiRuntimeInit(ChiRuntime* rt, ChiExitHandler exitHandler) {
    CHI_ZERO_STRUCT(rt);
    chiRuntimeOptionDefaults(&rt->option);
    rt->timeRef.start = chiClockFine();
    rt->exitHandler = exitHandler;
}

static ChiOptionResult parseOptions(ChiRuntimeOption* opt, ChiChunkOption* chunkOption, ChiProfOption* profOption,
                         int* argc, char** argv) {
    CHI_NOWARN_UNUSED(profOption);
    CHI_NOWARN_UNUSED(chunkOption);
    CHI_AUTO_SINK(helpSink, chiSinkColor(CHI_SINK_COLOR_AUTO));
    ChiOptionParser parser =
        { .help = helpSink,
          .usage = chiStderr,
          .assocs = (ChiOptionAssoc[])
          { { &chiGeneralOptions, opt },
            { &chiStackOptions, opt },
            { &chiNurseryOptions, opt },
            { &chiHeapOptions, opt },
            { &chiGCOptions, opt },
            CHI_IF(CHI_SYSTEM_HAS_MALLOC, { &chiChunkOptions, chunkOption },)
            CHI_IF(CHI_EVENT_ENABLED, { &chiEventOptions, opt },)
            CHI_IF(CHI_STATS_ENABLED, { &chiStatsOptions, opt },)
            CHI_IF(CHI_PROF_ENABLED, { &chiProfOptions, profOption },)
            { 0 }
          }
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
