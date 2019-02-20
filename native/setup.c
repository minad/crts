#include "bigint.h"
#include "runtime.h"
#include "stack.h"
#include "num.h"
#include "adt.h"
#include "chunk.h"
#include "event.h"
#include "thread.h"
#include "export.h"
#include "error.h"
#include "sink.h"
#include "profiler.h"
#include <stdlib.h>

#define NOVEC
#define VEC ChiHookVec
#define VEC_TYPE ChiHook
#define VEC_PREFIX chiHookVec
#include "vector.h"

typedef struct {
    size_t hard, soft;
} HeapLimits;

CHI_INL HeapLimits heapLimits(const ChiRuntimeOption* opt) {
    return (HeapLimits) {
        .hard = (size_t)(((uint64_t)opt->heap.limit.hard * opt->heap.limit.size) / 100),
        .soft = (size_t)(((uint64_t)opt->heap.limit.soft * opt->heap.limit.size) / 100)
    };
}

static bool heapLimitReached(ChiRuntime* rt, size_t size) {
    return rt->heapSize + size > heapLimits(&rt->option).hard;
}

static size_t heapGrow(ChiRuntime* rt, size_t size) {
    rt->heapSize += size;
    size_t heapSize = rt->heapSize;
    HeapLimits limits = heapLimits(&rt->option);
    if (heapSize > limits.soft || heapSize > limits.hard) {
        if (heapSize > limits.hard)
            rt->heapOverflow = true;
        CHI_EVENT(rt, HEAP_LIMIT,
                  .heapSize = heapSize,
                  .softLimit = limits.soft,
                  .hardLimit = limits.hard,
                  .limit = heapSize > limits.hard ? CHI_HEAP_LIMIT_HARD : CHI_HEAP_LIMIT_SOFT);
        chiGCTrigger(rt, CHI_GC_REQUESTOR_RUNTIME, CHI_GC_TRIGGER_FULL, CHI_NODUMP);
    }
    return heapSize;
}

static size_t heapShrink(ChiRuntime* rt, size_t size) {
    rt->heapSize -= size;
    return rt->heapSize;
}

void chiBlockManagerLimitReached(ChiBlockManager* CHI_UNUSED(bm)) {
    chiGCTrigger(CHI_CURRENT_RUNTIME, CHI_GC_REQUESTOR_BLOCKMAN, CHI_GC_TRIGGER_SLICE, CHI_NODUMP);
}

ChiChunk* chiBlockManagerChunkNew(ChiBlockManager* CHI_UNUSED(bm), size_t size, size_t align) {
    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    ChiChunk* chunk = chiChunkNew(size, align);
    /* Block allocations must never fail.
     * If we cannot allocate the fairly small chunks used by
     * the block manager, the system must be under immense pressure
     * and termination is the only reasonable thing to do.
     */
    if (!chunk)
        chiErr("Block manager failed to allocate chunk: size=%Z align=%Z heapSize=%Z: %m", size, align, rt->heapSize);
    size_t heapSize = heapGrow(rt, size);
    CHI_EVENT(rt, BLOCK_CHUNK_NEW, .size = size, .align = align,
              .start = (uintptr_t)chunk->start, .heapSize = heapSize);
    return chunk;
}

void chiBlockManagerChunkFree(ChiBlockManager* CHI_UNUSED(bm), ChiChunk* chunk) {
    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    size_t heapSize = heapShrink(rt, chunk->size);
    CHI_EVENT(rt, BLOCK_CHUNK_FREE, .size = chunk->size, .start = (uintptr_t)chunk->start,
              .heapSize = heapSize);
    chiChunkFree(chunk);
}

ChiChunk* chiHeapChunkNew(ChiHeap* heap, size_t size, ChiHeapFail fail) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    if (fail == CHI_HEAP_MAYFAIL && heapLimitReached(rt, size))
        return 0;
    ChiChunk* chunk = chiChunkNew(size, 0);
    if (!chunk) {
        if (fail == CHI_HEAP_MAYFAIL)
            return 0;
        chiErr("Heap failed to allocate chunk: size=%Z heapSize=%Z: %m", size, rt->heapSize);
    }
    size_t heapSize = heapGrow(rt, size);
    CHI_EVENT(rt, HEAP_CHUNK_NEW,
              .size = size, .start = (uintptr_t)chunk->start, .heapSize = heapSize);
    return chunk;
}

void chiHeapChunkFree(ChiHeap* heap, ChiChunk* chunk) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    size_t heapSize = heapShrink(rt, chunk->size);
    CHI_EVENT(rt, HEAP_CHUNK_FREE, .size = chunk->size, .start = (uintptr_t)chunk->start,
              .heapSize = heapSize);
    chiChunkFree(chunk);
}

void chiHeapAllocLimitReached(ChiHeap* heap) {
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, heap, heap);
    HeapLimits limits = heapLimits(&rt->option);
    CHI_EVENT(rt, HEAP_LIMIT,
              .heapSize = rt->heapSize,
              .softLimit = limits.soft,
              .hardLimit = limits.hard,
              .limit = CHI_HEAP_LIMIT_ALLOC);
    chiGCTrigger(rt, CHI_GC_REQUESTOR_HEAP, CHI_GC_TRIGGER_MARKSWEEP, CHI_NODUMP);
}

static ChiMicros timerHandler(ChiTimer* timer) {
    ChiInterruptSupport* is = CHI_OUTER(ChiInterruptSupport, timer, timer);
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, interruptSupport[0], is);
    CHI_EVENT0(rt, TICK);
    if (CHI_EVENT_P(rt, ACTIVITY)) {
        ChiActivity activity;
        chiActivity(&activity);
        CHI_EVENT_STRUCT(rt, ACTIVITY, &activity);
    }
    chiProcessorTick(rt);
    return timer->timeout;
}

static void forcedExit(void) {
    chiErr("Forced runtime exit by user interrupt.");
}

static void userInterrupt(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;
    ChiInterruptSupport* is = rt->interruptSupport;
    if (!opt->interruptLimit)
        forcedExit();
    if (++is->userInterrupt > 1) {
        if (is->userInterrupt >= opt->interruptLimit)
            forcedExit();
        if (is->userInterrupt + 1 == opt->interruptLimit)
            chiWarn("Sorry, I am locked up in the runtime system. Interrupt once more!");
        else
            chiWarn("Sorry, I am locked up in the runtime system. Interrupt %u times...",
                      opt->interruptLimit - is->userInterrupt);
    }
}

static void signalHandler(ChiSigHandler* handler) {
    ChiInterruptSupport* is = CHI_OUTER(ChiInterruptSupport, sigHandler[0], handler - handler->sig);
    ChiRuntime* rt = CHI_OUTER(ChiRuntime, interruptSupport[0], is);
    CHI_EVENT(rt, SIGNAL, .sig = handler->sig);

    switch (handler->sig) {
    case CHI_SIG_INTERRUPT:
        userInterrupt(rt);
        // fall through

    case CHI_SIG_DUMPSTACK:
        chiProcessorSignal(rt, handler->sig);
        break;

    case CHI_SIG_DUMPHEAP:
        chiGCTrigger(rt, CHI_GC_REQUESTOR_SIG, CHI_GC_TRIGGER_SLICE, CHI_DUMP);
        break;
    }
}

static void hookFree(ChiRuntime* rt) {
    for (size_t i = 0; i < CHI_DIM(rt->hooks); ++i)
        chiHookVecFree(rt->hooks + i);
}

void chiHook(ChiRuntime* rt, ChiHookType type, ChiHookFn fn) {
    chiHookVecPush(rt->hooks + type, (ChiHook){ .type = type, .fn = fn });
}

void chiHookRun(ChiWorker* worker, ChiHookType type) {
    const ChiHookVec* v = worker->rt->hooks + type;
    for (size_t i = v->used; i --> 0;)
        CHI_VEC_AT(v, i).fn(worker, type);
}

static void setupStack(ChiProcessor* proc) {
    ChiStack* stack = chiToStack(chiThreadStackUnchecked(proc->thread));
    *stack->sp++ = chiFromCont(&chiRunMainCont);
    *stack->sp++ = chiFromCont(&z_Main);
    *stack->sp++ = chiFromCont(&chiJumpCont);
    *stack->sp++ = chiFromCont(&chiStartupEndCont);

    const ChiRuntimeOption* opt = &proc->rt->option;
    if (!chiMillisZero(opt->interval)) {
        *stack->sp++ = chiFromCont(&chiSetupEntryPointsCont);
        *stack->sp++ = chiFromCont(&z_System_2fEntryPoints);
        *stack->sp++ = chiFromCont(&chiJumpCont);
    }

    *stack->sp = chiFromCont(&chiIdentity);
}

static void setupInterrupt(ChiInterruptSupport* is, ChiMillis interval) {
    if (!CHI_SYSTEM_HAS_INTERRUPT)
        return;
    for (uint32_t i = 0; i < CHI_DIM(is->sigHandler); ++i) {
        is->sigHandler[i] = (ChiSigHandler){ .sig = i, .handler = signalHandler };
        chiSigInstall(is->sigHandler + i);
    }
    is->timer = (ChiTimer){
        .handler = timerHandler,
        .timeout = chiMillisToMicros(interval),
    };
    if (!chiMillisZero(interval))
        chiTimerInstall(&is->timer);
}

static void destroyInterrupt(ChiInterruptSupport* is) {
    if (!CHI_SYSTEM_HAS_INTERRUPT)
        return;
    for (size_t i = 0; i < CHI_DIM(is->sigHandler); ++i)
        chiSigRemove(is->sigHandler + i);
    chiTimerRemove(&is->timer);
}

static void setupClosures(ChiRuntime* rt) {
    rt->blackhole = chiRoot(rt, chiNewPinVariadic(CHI_THUNKFN, chiFromCont(&chiBlackhole)));
    rt->switchThreadClos = chiRoot(rt, chiNewPinFn(1, &chiSwitchThread));
    rt->enterContClos = chiRoot(rt, chiNewPinFn(2, &chiEnterCont));
    rt->entryPoint.unhandled = chiRoot(rt, chiNewPinFn(2, &chiEntryPointUnhandledDefault));
    rt->entryPoint.exit = chiRoot(rt, chiNewPinFn(1, &chiEntryPointExitDefault));
    rt->entryPoint.interrupt = chiRoot(rt, chiNewPinFn(1, &chiEntryPointExitDefault));
}

static void setup(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;
    chiStatsSetup(&rt->stats, opt->stats->tableCell, opt->stats->json);
    chiEventSetup(rt);
    ChiProcessor* proc = chiProcessorBegin(rt);
    chiBlockManagerSetup(&rt->blockTemp, opt->block.size, opt->block.chunk, 0);
    ChiHeapDesc small = {
        .chunk = opt->heap.small.chunk / CHI_WORDSIZE,
        .max    = opt->heap.small.max / CHI_WORDSIZE,
        .sub    = chiLog2(opt->heap.small.sub)
    };
    ChiHeapDesc medium = {
        .chunk = opt->heap.medium.chunk / CHI_WORDSIZE,
        .max    = opt->heap.medium.max / CHI_WORDSIZE,
        .sub    = chiLog2(opt->heap.medium.sub)
    };
    chiHeapSetup(&rt->heap, &small, &medium, opt->heap.limit.alloc);
    chiTenureSetup(&rt->tenure, opt->block.size, opt->block.chunk, opt->block.gen);
    chiGCSetup(rt);
    chiBigIntSetup();
    chiProcessorSetup(proc);
    setupClosures(rt);
    setupStack(proc);
    setupInterrupt(rt->interruptSupport, opt->interval);
}

void chiDestroy(ChiRuntime* rt) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(chiMainWorker(&proc->worker));
    CHI_ASSERT(proc->rt == rt);
    const ChiRuntimeOption* opt = &rt->option;

    chiSinkFlush(chiStdout);
    chiSinkFlush(chiStderr);

    destroyInterrupt(rt->interruptSupport);
    chiGCDestroy(rt);
    chiHookRun(&proc->worker, CHI_HOOK_RUNTIME_DESTROY);
    chiTenureDestroy(&rt->tenure, opt->block.gen);
    chiBlockManagerDestroy(&rt->blockTemp);
    chiHeapDestroy(&rt->heap);
    chiFree(rt->moduleHash.entry);
    hookFree(rt);
    chiEventDestroy(rt);
    chiStatsDestroy(&rt->stats, opt->stats->file, opt->color);
}

void chiRun(ChiRuntime* rt, int argc, char** argv) {
    ChiRuntimeOption* opt = &rt->option;
    chiRuntimeOptionValidate(opt);

    CHI_ASSERT(argc >= 0);
    rt->argc = (uint32_t)argc;
    rt->argv = argv;

    setup(rt);
    chiEnter();
}

uint64_t chiCurrentTimeNanos(void) {
    return CHI_UN(Nanos, chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), CHI_CURRENT_RUNTIME->timeRef.start.real));
}

uint64_t chiCurrentTimeMillis(void) {
    return CHI_UN(Millis, chiNanosToMillis(chiNanosDelta(chiClock(CHI_CLOCK_REAL_FAST), CHI_CURRENT_RUNTIME->timeRef.start.real)));
}

void chiInit(ChiRuntime* rt) {
    CHI_CLEAR(rt);
    chiRuntimeOptionDefaults(&rt->option);
    rt->exitCode = -1;
    rt->timeRef.start = chiTime();
}

CHI_WARN_OFF(frame-larger-than=)
_Noreturn void chiMain(int argc, char** argv) {
    ChiRuntime rt;
    chiRuntimeMain(&rt, argc, argv);
}
CHI_WARN_ON

// We allow to use more stack space here to store
// the rather large ChiRuntime struct.
_Noreturn void chiRuntimeMain(ChiRuntime* rt, int argc, char** argv) {
    chiSystemSetup();

    ChiChunkOption chunkOption = CHI_DEFAULT_CHUNK_OPTION;
    ChiProfOption profOption = CHI_DEFAULT_PROF_OPTION;
    chiInit(rt);

    {
        CHI_AUTO_SINK(coloredStdout, chiStdoutColor(CHI_SINK_COLOR_AUTO));
        const ChiOptionList list[] =
            { { chiRuntimeOptionList, &rt->option },
              CHI_IF(CHI_SYSTEM_HAS_MALLOC, { chiChunkOptionList, &chunkOption },)
              CHI_IF(CHI_PROF_ENABLED, { chiProfOptionList, &profOption },)
              { 0, 0 }
            };
        const ChiOptionParser parser =
            { .out = coloredStdout,
              .err = chiStderr,
              .list = list
            };
        ChiOptionResult res = chiOptionEnv(&parser, "CHI_NATIVE_OPTS");
        if (res != CHI_OPTRESULT_OK)
            exit(res == CHI_OPTRESULT_ERROR ? CHI_EXIT_ERROR : CHI_EXIT_SUCCESS);
        res = chiOptionArgs(&parser, &argc, argv);
        if (res == CHI_OPTRESULT_HELP) {
            chiPager();
            chiOptionHelp(&parser);
        }
        if (res != CHI_OPTRESULT_OK)
            exit(res == CHI_OPTRESULT_ERROR ? CHI_EXIT_ERROR : CHI_EXIT_SUCCESS);
    }

    if (CHI_SYSTEM_HAS_MALLOC)
        chiChunkSetup(&chunkOption);

    if (CHI_PROF_ENABLED && (profOption.flat || profOption.file[0]))
        chiProfilerSetup(rt, &profOption);

    chiRun(rt, argc, argv);
}

#ifndef CHI_MODE
#  error CHI_MODE must be defined
#endif

// Include the compilation mode into the binary
extern CHI_API const bool CHI_CAT(chi_mode_, CHI_MODE);
CHI_API const __attribute__ ((used)) bool CHI_CAT(chi_mode_, CHI_MODE) = true;
