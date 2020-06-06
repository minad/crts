#include "chunk.h"
#include "color.h"
#include "error.h"
#include "event.h"
#include "num.h"
#include "sink.h"
#include "strutil.h"
#include "version.h"

#define FILE_NAMES      "file, stdout, stderr, null, fd:n"
#define FILTER_EXAMPLES "Examples: HELP, -ALL+TICK, THR+GC+TI"
#define LOGO "  "FgGreen"l"FgDefault"\n "FgRed"("FgRed"`"FgRed"\\"FgDefault"  Chili\n  "FgRed"`.\\"FgDefault"   Native\n     "FgRed"`"FgDefault"\n"

#ifdef __pie__
#  define VERSION_PIE "on"
#else
#  define VERSION_PIE "off"
#endif

static const ChiRuntimeOption defaultOption = {
    .interval          = chiMillis(20),
    .stack = {
        .step          = CHI_KiB(32) - 1,
        .init          = 512 - 1,
        .hot           = CHI_KiB(1),
        .trace         = 40,
    },
    .gc = {
           .aging = 1,
           .slice = chiMicros(1000),
           .mode  = CHI_CHOICE(CHI_GC_CONC_ENABLED, CHI_GC_CONC, CHI_GC_INC),
           .gray = {
                    .block         = CHI_KiB(4),
                    .chunk         = CHI_MiB(2),
           },
    },
    .nursery = {
        .block         = CHI_KiB(8),
        .chunk         = CHI_MiB(2),
        .limit         = CHI_KiB(512),
    },
    .heap = {
        .limit = {
            .init  = CHI_MiB(16),
            .overflow  = 90,
            .full  = 80,
        },
        .scanDepth     = 30,
        .small = {
            .segment   = CHI_MiB(1),
            .page      = CHI_KiB(8),
        },
        .medium = {
            .segment   = CHI_MiB(1),
            .page      = CHI_KiB(64),
        },
    },
    CHI_IF(CHI_STATS_ENABLED, .stats = {
        .tableRows     = 20,
        .tableCell     = 20,
        .bins          = 64,
        .scale         = 140,
    },)
    CHI_IF(CHI_EVENT_ENABLED, .event = {
        .filter        = { [0 ... (_CHI_EVENT_FILTER_SIZE - 1) ] = ~(uintptr_t)0 },
        .msgSize       = CHI_KiB(4),
        .bufSize       = 100,
    },)
};

#ifndef CHI_MODE
#  error CHI_MODE must be defined
#endif
#ifndef CHI_TARGET
#  error CHI_TARGET must be defined
#endif

// Include the version info into the binary
#define CHI_VERSION_SYMBOL CHI_CAT4(chi_version_, CHI_TARGET, _, CHI_MODE)

extern CHI_EXPORT const char CHI_VERSION_SYMBOL[];
CHI_EXPORT __attribute__ ((used)) const char CHI_VERSION_SYMBOL[] =
    LOGO CHI_VERSION_INFO
    "\nTarget:   " CHI_STRINGIZE(CHI_TARGET)
    "\nMode:     " CHI_STRINGIZE(CHI_MODE)
    "\nPIE:      " VERSION_PIE
    "\nArch:     " CHI_STRINGIZE(CHI_ARCH)
    "\nCallconv: " CHI_STRINGIZE(CHI_CALLCONV)
    "\nCompiler: " CHI_CC_VERSION_STRING
    "\nFFI:      " CHI_STRINGIZE(CBY_FFI_BACKEND)
    "\nBoxing:   " CHI_CHOICE(CHI_NANBOXING_ENABLED, "nanbox", "ptrtag")
    "\nCont:     " CHI_CHOICE(CHI_CONT_PREFIX, "prefix", "indirect")
    "\nDispatch: " CHI_STRINGIZE(CBY_DISPATCH) "\n";

static ChiOptionResult showVersion(ChiOptionParser* parser,
                                   ChiOptionAssoc* CHI_UNUSED(assoc),
                                   ChiOption* CHI_UNUSED(opt),
                                   const void* CHI_UNUSED(val)) {
    chiSinkPuts(parser->help, CHI_VERSION_SYMBOL);
    return CHI_OPTRESULT_EXIT;
}

#if CHI_EVENT_ENABLED
static ChiOptionResult setEventFilter(ChiOptionParser* parser,
                                      ChiOptionAssoc* assoc,
                                      ChiOption* CHI_UNUSED(opt),
                                      const void* val) {
    const char* s = (const char*)val;

    if (streq(s, "HELP")) {
        chiSinkPuts(parser->help,
                    "Events can be enabled (disabled) with '+PREFIX' ('-PREFIX').\n"
                    "The prefix might match multiple event names and 'ALL' refers to all events.\n"
                    FILTER_EXAMPLES "\n"
                    "List of events:\n");
        for (size_t i = 0; i < _CHI_EVENT_COUNT; ++i) {
            if (i == 0 || !streq(chiEventName[i - 1], chiEventName[i]))
                chiSinkFmt(parser->help, "  %s\n", chiEventName[i]);
        }
        return CHI_OPTRESULT_EXIT;
    }

    return chiEventModifyFilter(((ChiRuntimeOption*)assoc->target)->event.filter, chiStringRef(s))
        ? CHI_OPTRESULT_OK : CHI_OPTRESULT_ERROR;
}
#endif

static void computeOptions(ChiRuntimeOption* opt) {
    CHI_NOWARN_UNUSED(opt);

#if CHI_EVENT_ENABLED
    if (CHI_AND(CHI_EVENT_PRETTY_ENABLED, opt->event.pretty) || opt->event.file[0])
        opt->event.enabled = true;
    if (opt->event.enabled && !opt->event.file[0]) {
#ifdef CHI_STANDALONE_SANDBOX
        memcpy(opt->event.file, "3", 2);
#else
        chiFmt(opt->event.file, sizeof (opt->event.file), "event.%u.%s",
               chiPid(), CHI_AND(CHI_EVENT_PRETTY_ENABLED, opt->event.pretty) ? "pretty" : "evlog");
#endif
    }
#endif

#if CHI_STATS_ENABLED
    if (opt->stats.file[0] || opt->stats.json || opt->stats.histogram)
        opt->stats.enabled = true;
    if (opt->stats.json)
        opt->stats.histogram = true;
#endif

#if CHI_GC_CONC_ENABLED
    if (opt->gc.mode == CHI_GC_CONC && !opt->gc.worker)
        opt->gc.mode = CHI_GC_INC;
    if (opt->gc.mode == CHI_GC_INC)
        opt->gc.worker = 0;
    if (opt->gc.mode == CHI_GC_CONC)
        opt->gc.slice = chiMicros(0);
#endif
}

static const char* validateBlockSize(size_t block, size_t chunk) {
    if (!chiIsPow2(block))
        return "Block size must be a power of two.";

    if (!chiChunkSizeValid(chunk))
        return "Block chunk size is invalid.";

    if (block >= chunk)
        return "Block size must be smaller than chunk size.";

    return 0;
}

static const char* validateOptions(const ChiRuntimeOption* opt) {
    const char* err = validateBlockSize(opt->nursery.block, opt->nursery.chunk);
    if (err)
        return err;

    err = validateBlockSize(opt->gc.gray.block, opt->gc.gray.chunk);
    if (err)
        return err;

    if (opt->nursery.limit % opt->nursery.block)
        return "Nursery limit must be a multiple of the block size.";

    if (opt->nursery.limit < 8 * opt->nursery.block)
        return "Nursery limit is too small.";

    if (opt->stack.init >= opt->stack.limit)
        return "Initial stack size must be smaller than limit.";

    if (opt->stack.init >= opt->stack.step)
        return "Initial stack size must be smaller than step size.";

    if (opt->stack.step >= opt->stack.limit)
        return "Stack step size must be smaller than limit.";

    if (opt->stack.hot >= opt->stack.step / 2)
        return "Stack hot area is too large.";

    if (!chiChunkSizeValid(opt->heap.small.segment) || !chiChunkSizeValid(opt->heap.medium.segment))
        return "Heap segment size is invalid.";

    if (!chiIsPow2(opt->heap.small.page) || !chiIsPow2(opt->heap.medium.page))
        return "Heap page size must be a power of two.";

    if (opt->heap.small.segment <= opt->heap.small.page
        || opt->heap.medium.segment <= opt->heap.medium.page)
        return "Heap segment size must be larger than page size.";

    return 0;
}

void chiRuntimeOptionValidate(ChiRuntimeOption* opt) {
    computeOptions(opt);
    if (CHI_OPTION_ENABLED) {
        const char* err = validateOptions(opt);
        if (err)
            chiErr("%s", err);
    }
}

void chiRuntimeOptionDefaults(ChiRuntimeOption* opt) {
    *opt = defaultOption;
    CHI_IF(CHI_SYSTEM_HAS_TASK, opt->processors = chiPhysProcessors());
    CHI_IF(CHI_GC_CONC_ENABLED, opt->gc.worker = CHI_MAX(1U, opt->processors / 4));
    uint64_t max = CHI_MIN(3 * chiPhysMemory() / 4, CHI_ARCH_32BIT ? CHI_GiB(2) : CHI_GiB(128));
    opt->heap.limit.max = (size_t)(max & CHI_ALIGNMASK(CHI_MiB(1)));
    opt->stack.limit = (opt->heap.limit.max / 2) & CHI_ALIGNMASK(CHI_MiB(1));
    computeOptions(opt);
}

#define CHOICE_NAME(N, n) #n
#define CHI_OPT_STRUCT ChiRuntimeOption
CHI_INTERN CHI_OPT_GROUP(chiGeneralOptions, "RUNTIME",
                         CHI_IF(CHI_SYSTEM_HAS_TASK, CHI_OPT_UINT32(processors, 1, 1000, "p", "Number of processors"))
                         CHI_OPT_UINT64(_CHI_UN(Millis, interval), 1, 10000, "i", "Scheduling interval in ms")
                         CHI_IF(CHI_COLOR_ENABLED,
                                CHI_OPT_CHOICE(color, "color", "Enable terminal colors", CHI_FOREACH_SINK_COLOR(CHOICE_NAME, ", ")))
                         CHI_OPT_CB(FLAG, showVersion, "version", "Show version information"))
CHI_INTERN CHI_OPT_GROUP(chiStackOptions, "STACK",
                         CHI_OPT_SIZE(stack.limit, CHI_KiB(1), CHI_GiB(1), "s", "Stack size limit")
                         CHI_OPT_SIZE(stack.init, 100, CHI_MiB(64), "s:i", "Initial stack size")
                         CHI_OPT_SIZE(stack.step, 100, CHI_MiB(64), "s:g", "Maximal stack growth step")
                         CHI_OPT_SIZE(stack.hot, 0, CHI_MiB(1), "s:h", "Stack hot area")
                         CHI_OPT_UINT32(stack.trace, 0, 1000000, "s:t", "Stacktrace depth")
                         CHI_OPT_FLAG(stack.traceMangled, "s:tm", "Print mangled stack traces")
                         CHI_OPT_FLAG(stack.traceCycles, "s:tc", "Print repeating cycles in stack traces"))
CHI_INTERN CHI_OPT_GROUP(chiNurseryOptions, "NURSERY",
                         CHI_OPT_SIZE(nursery.limit, CHI_KiB(8), CHI_GiB(1), "n", "Nursery limit")
                         CHI_OPT_SIZE(nursery.block, CHI_WORDSIZE * CHI_BLOCK_MINSIZE, CHI_WORDSIZE * CHI_BLOCK_MAXSIZE,
                                      "n:b", "Nursery block size")
                         CHI_OPT_SIZE(nursery.chunk, CHI_CHUNK_MIN_SIZE, CHI_GiB(1), "n:c", "Nursery chunk size"))
CHI_INTERN CHI_OPT_GROUP(chiHeapOptions, "HEAP",
                         CHI_OPT_SIZE(heap.limit.max, CHI_MiB(32), CHI_CHUNK_MAX_SIZE, "h", "Maximal heap size")
                         CHI_OPT_SIZE(heap.limit.init, CHI_MiB(1), CHI_GiB(1), "h:i", "Initial heap size")
                         CHI_OPT_UINT32(heap.limit.overflow, 10, 100, "h:lo", "Heap overflow limit, Percent of maximal size")
                         CHI_OPT_UINT32(heap.limit.full, 10, 100, "h:lg", "Heap GC limit, Percent of current total size")
                         CHI_OPT_SIZE(heap.small.segment, CHI_KiB(64), CHI_MiB(32), "h:ss", "Heap small segment size")
                         CHI_OPT_SIZE(heap.small.page, CHI_KiB(1), CHI_MiB(1), "h:sp", "Heap small page size")
                         CHI_OPT_SIZE(heap.medium.segment, CHI_KiB(64), CHI_MiB(32), "h:ms", "Heap medium segment size")
                         CHI_OPT_SIZE(heap.medium.page, CHI_KiB(64), CHI_MiB(1), "h:mp", "Heap medium page size")
                         CHI_OPT_UINT32(heap.scanDepth, 0, 10000, "h:d", "Heap scan recursion depth")
                         CHI_OPT_FLAG(heap.noCard, "h:nocard", "Disable card tables"))
CHI_INTERN CHI_OPT_GROUP(chiGCOptions, "GC",
                         CHI_OPT_CHOICE(gc.mode, "gc", "Choose collection mode", CHI_FOREACH_GCMODE(CHOICE_NAME, ", "))
                         CHI_OPT_UINT64(_CHI_UN(Micros, gc.slice), 100, 1000000, "gc:t", "Time slice in µs of incremental major GC")
                         CHI_IF(CHI_GC_CONC_ENABLED, CHI_OPT_UINT32(gc.worker, 0, 100, "gc:w", "Number of workers"))
                         CHI_OPT_SIZE(gc.gray.block, CHI_WORDSIZE * CHI_BLOCK_MINSIZE, CHI_WORDSIZE * CHI_BLOCK_MAXSIZE,
                                      "gc:gb", "Gray list block size")
                         CHI_OPT_SIZE(gc.gray.chunk, CHI_CHUNK_MIN_SIZE, CHI_GiB(1), "gc:gc", "Gray list chunk size")
                         CHI_OPT_UINT32(gc.aging, 0, CHI_GEN_MAJOR - 1, "gc:a", "Aging of objects in minor heap"))
CHI_IF(CHI_EVENT_ENABLED,
       CHI_INTERN CHI_OPT_GROUP(chiEventOptions, "EVENT",
                                CHI_OPT_FLAG(event.enabled, "ev", "Write binary event log")
                                CHI_IF(CHI_EVENT_PRETTY_ENABLED, CHI_OPT_FLAG(event.pretty, "ev:h", "Write event log in human readable format"))
                                CHI_OPT_STRING(event.file, "ev:o", "Set log output ("FILE_NAMES")")
                                CHI_OPT_CB(STRING, setEventFilter, "ev:f", "Event filter ("FILTER_EXAMPLES")")
                                CHI_OPT_SIZE(event.msgSize, 512, CHI_KiB(64), "ev:m", "Maximal message size")
                                CHI_OPT_UINT32(event.bufSize, 0, 1000, "ev:b", "Buffer size in messages")))
CHI_IF(CHI_STATS_ENABLED,
       CHI_INTERN CHI_OPT_GROUP(chiStatsOptions, "STATISTICS",
                                CHI_OPT_FLAG(stats.enabled, "stat", "Print statistics")
                                CHI_OPT_FLAG(stats.histogram, "stat:h", "Print histograms")
                                CHI_OPT_FLAG(stats.json, "stat:json", "Output as json")
                                CHI_OPT_STRING(stats.file, "stat:o", "Set statistics output ("FILE_NAMES")")
                                CHI_OPT_UINT32(stats.tableRows, 1, 10000, "stat:tr", "Table rows")
                                CHI_OPT_UINT32(stats.tableCell, 1, 1000, "stat:tc", "Table cell size")
                                CHI_OPT_UINT32(stats.bins, 1, 1000, "stat:hb", "Histogram bins")
                                CHI_OPT_UINT32(stats.scale, 110, 1000, "stat:hs", "Histogram log scale")
                                CHI_OPT_FLAG(stats.cumulative, "stat:hc", "Cumulative histogram")))
#undef CHI_OPT_STRUCT
