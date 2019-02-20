#include "rtoption.h"
#include "system.h"
#include "event.h"
#include "chunk.h"
#include "error.h"
#include "sink.h"
#include "strutil.h"
#include "version.h"
#include "color.h"
#include "num.h"

#define FILE_TYPES                              \
    "file, stdout, stderr"                      \
    CHI_IF(CHI_SINK_FD_ENABLED,   ", fd:n")
#define FILTER_EXAMPLES "Examples: HELP, -ALL+TICK, THR+GC+TI"
#define LOGO "  "FgGreen"l"FgDefault"\n "FgRed"("FgRed"`"FgRed"\\"FgDefault"  Chili\n  "FgRed"`.\\"FgDefault"   Native\n     "FgRed"`"FgDefault"\n"

#ifdef __clang__
#  define VERSION_COMPILER "clang " CHI_STRINGIZE(__clang_major__) "." CHI_STRINGIZE(__clang_minor__)
#elif defined (__GNUC__)
#  define VERSION_COMPILER "gcc " CHI_STRINGIZE(__GNUC__) "." CHI_STRINGIZE(__GNUC_MINOR__)
#else
#  error Unsupported Compiler
#endif

#ifdef __pie__
#  define VERSION_PIE "on"
#else
#  define VERSION_PIE "off"
#endif

static const ChiRuntimeOption defaultOption = {
    .interval          = (ChiMillis){20},
    .interruptLimit    = 3,
    .stack = {
        .growth        = 200,
        .max           = 1e7,
        .init          = 60, // not a power of two, to avoid to much waste in 2^n chunks,
        .trace         = 40,
    },
    .gc = {
        .mode          = CHI_IFELSE(CHI_SYSTEM_HAS_TASKS, CHI_GC_CONC, CHI_GC_INC),
        .scav = {
            .multiplicity  = 2,
        },
        .ms = {
            .slice     = (ChiMicros){1000},
            .noCollapse = false,
        },
    },
    .block = {
        .size          = CHI_KiB(8),
        .chunk         = CHI_MiB(2),
        .nursery       = 128,
        .gen           = 3,
    },
    .heap = {
        CHI_IF(CHI_HEAP_PROF_ENABLED, .prof = CHI_GEN_MAX,)
        .limit = {
            .hard  = 90,
            .soft  = 80,
            .alloc = 50,
        },
        .scanDepth     = 30,
        .small = {
            .sub       = 2,
            .max       = CHI_KiB(1),
            .chunk     = CHI_MiB(2),
        },
        .medium = {
            .sub      = 8,
            .max      = CHI_CHUNK_MIN_SIZE,
            .chunk    = CHI_MiB(2),
        },
    },
    .evalCount = 100000,
#if CHI_STATS_ENABLED
    .stats[0] = {
        .file          = "stderr",
        .tableRows     = 30,
        .tableCell     = 30,
        .bins          = 64,
        .scale         = 140,
    },
#endif
#if CHI_EVENT_ENABLED
    .event[0] = {
        .msgSize       = CHI_KiB(4),
        .bufSize       = 16,
    },
#endif
};

static ChiOptionResult showVersion(const ChiOptionParser* parser,
                                   const ChiOptionList* CHI_UNUSED(list),
                                   const ChiOption* CHI_UNUSED(opt),
                                   const void* CHI_UNUSED(val)) {
    chiSinkPuts(parser->out,
                LOGO CHI_VERSION_INFO
                "\nTarget:   " CHI_STRINGIZE(CHI_TARGET)
                "\nMode:     " CHI_STRINGIZE(CHI_MODE)
                "\nPIE:      " VERSION_PIE
                "\nArch:     " CHI_STRINGIZE(CHI_ARCH)
                "\nCallconv: " CHI_STRINGIZE(CHI_CALLCONV)
                "\nCompiler: " VERSION_COMPILER
                "\nBigint:   " CHI_STRINGIZE(CHI_BIGINT_BACKEND)
                "\nFFI:      " CHI_STRINGIZE(CBY_FFI_BACKEND)
                "\nBoxing:   " CHI_IFELSE(CHI_NANBOXING_ENABLED, "nanbox", "ptrtag")
                "\nDispatch: " CHI_STRINGIZE(CBY_DISPATCH) "\n");
    return CHI_OPTRESULT_EXIT;
}

#if CHI_EVENT_ENABLED
_Static_assert(_CHI_EVENT_COUNT < 8 * CHI_EVENT_FILTER_SIZE, "CHI_EVENT_FILTER_SIZE is too small");

static ChiOptionResult setEventFilter(const ChiOptionParser* parser,
                                      const ChiOptionList* list,
                                      const ChiOption* CHI_UNUSED(opt),
                                      const void* val) {
    const char* s = (const char*)val;

    if (streq(s, "HELP")) {
        chiSinkPuts(parser->out,
                    "Events can be enabled (disabled) with '+PREFIX' ('-PREFIX').\n"
                    "The prefix might match multiple event names and 'ALL' refers to all events.\n"
                    FILTER_EXAMPLES "\n"
                    "List of events:\n");
        for (size_t i = 0; i < _CHI_EVENT_COUNT; ++i) {
            if (i == 0 || !streq(chiEventName[i - 1], chiEventName[i]))
                chiSinkFmt(parser->out, "  %s\n", chiEventName[i]);
        }
        return CHI_OPTRESULT_EXIT;
    }

    return chiEventModifyFilter(((ChiRuntimeOption*)list->target)->event->filter, chiStringRef(s))
        ? CHI_OPTRESULT_OK : CHI_OPTRESULT_ERROR;
}
#endif

static void computeOptions(ChiRuntimeOption* opt) {
    // Scheduling disabled
    if (chiMillisZero(opt->interval)) {
        opt->blackhole = CHI_BH_NONE;
        opt->processors = 1;
    }

    if (CHI_EVENT_ENABLED && !opt->event->file[0] && opt->event->format) {
        if (CHI_DEFINED(CHI_STANDALONE_SANDBOX)) {
            memcpy(opt->event->file, "3", 2);
        } else {
#define _CHI_EVENTFORMAT(N, n, e) CHI_STRINGIZE(e),
            const char* const eventFormatExt[] = { CHI_FOREACH_EVENTFORMAT(_CHI_EVENTFORMAT,) };
#undef _CHI_EVENTFORMAT
            chiFmt(opt->event->file, sizeof (opt->event->file), "event.%u.%s%s",
                   chiPid(), eventFormatExt[opt->event->format], chiSinkCompress());
        }
    }

    if (CHI_STATS_ENABLED && opt->stats->json)
        opt->stats->enabled = 2;

    if (CHI_AND(CHI_SYSTEM_HAS_TASKS,
                opt->gc.mode == CHI_GC_CONC && !opt->gc.conc->markers && !opt->gc.conc->sweepers))
        opt->gc.mode = CHI_GC_INC;
}

static const char* validateOptions(const ChiRuntimeOption* opt) {
    if (opt->stack.init > opt->stack.max)
        return "Initial stack size must be smaller than maximal size.";

    if (!chiIsPow2(opt->block.size))
        return "Block size must be a power of two.";

    if (!chiChunkSizeValid(opt->block.chunk))
        return "Block chunk size is invalid.";

    if (opt->block.size >= opt->block.chunk)
        return "Block size must be smaller than chunk size.";

    if (!chiIsPow2(opt->heap.small.sub) || !chiIsPow2(opt->heap.medium.sub))
        return "Heap subdivisons must be a power of two.";

    if (opt->gc.scav.noPromote && opt->block.gen == 1)
        return "More than one block generation is needed if promotion is disabled.";

    if (!chiChunkSizeValid(opt->heap.small.chunk) || !chiChunkSizeValid(opt->heap.medium.chunk))
        return "Heap chunk size is invalid.";

    if (opt->heap.small.max >= opt->heap.small.chunk)
        return "Small object size must be smaller than chunk size.";

    if (opt->heap.medium.max >= opt->heap.medium.chunk)
        return "Medium object size must be smaller than chunk size.";

    if (opt->heap.medium.max < opt->heap.small.max)
        return "Medium object size must be larger than small object size.";

    if (chiPow(opt->gc.scav.multiplicity, opt->block.gen) > 1000000)
        return "Multiplicity^gen is too large.";

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
    if (CHI_EVENT_ENABLED)
        memset(opt->event->filter, 0xFF, sizeof (opt->event->filter));
    opt->processors = chiPhysProcessors();
    if (CHI_GC_CONC_ENABLED)
        opt->gc.conc->sweepers = opt->gc.conc->markers = CHI_MIN(opt->processors / 2, 4);
    opt->heap.limit.size =
        (size_t)CHI_MIN(3 * chiPhysMemory() / 4, CHI_ARCH_BITS == 32 ? CHI_GiB(2) : CHI_GiB(256));
    computeOptions(opt);
}

#define CHOICE_NAME(N, n) #n
#define EVENTFORMAT_NAME(N, n, e) #n
#define CHI_OPT_TARGET ChiRuntimeOption
const ChiOption chiRuntimeOptionList[] = {
    CHI_OPT_DESC_TITLE("RUNTIME")
    CHI_IF(CHI_SYSTEM_HAS_TASKS,
           CHI_OPT_DESC_UINT32(processors, 1, 1000, "p", "Number of processors"))
    CHI_IFELSE(CHI_SYSTEM_HAS_INTERRUPT,,
               CHI_OPT_DESC_UINT32(evalCount, 1, 1000000, "e", "Evaluation counter"))
    CHI_OPT_DESC_UINT64(_CHI_UN(Millis, interval), 0, 10000, "i", "Scheduling interval in ms")
    CHI_IF(CHI_COLOR_ENABLED,
           CHI_OPT_DESC_CHOICE(color, "color", "Enable terminal colors", CHI_FOREACH_COLOR(CHOICE_NAME, ", ")))
    CHI_OPT_DESC_CHOICE(blackhole, "bh", "Black holing mode", CHI_FOREACH_BH(CHOICE_NAME, ", "))
    CHI_OPT_DESC_FLAG(fastExit, "fex", "Enable fast exit")
    CHI_IF(CHI_SYSTEM_HAS_INTERRUPT,
           CHI_OPT_DESC_UINT32(interruptLimit, 0, 10, "uil", "User interrupt limit"))
    CHI_OPT_DESC_CB(FLAG, showVersion, "version", "Show version information")
    CHI_OPT_DESC_TITLE("STACK")
    CHI_OPT_DESC_UINT32(stack.max,  16, 1000000000, "s", "Maximal stack size")
    CHI_OPT_DESC_UINT32(stack.init, 16, 1000000000, "s:i", "Initial stack size")
    CHI_OPT_DESC_UINT32(stack.growth, 120, 300, "s:g", "Stack growth in percent")
    CHI_OPT_DESC_UINT32(stack.trace, 0, 1000000, "s:t", "Stacktrace depth")
    CHI_OPT_DESC_FLAG(stack.traceMangled, "s:tm", "Print mangled stack traces")
    CHI_OPT_DESC_TITLE("MINOR HEAP")
    CHI_OPT_DESC_UINT32(block.nursery, 2, 1000000, "b:n", "Nursery size in blocks")
    CHI_OPT_DESC_SIZE(block.size, CHI_WORDSIZE * CHI_BLOCK_MINSIZE, CHI_WORDSIZE * CHI_BLOCK_MAXSIZE,
                      "b:s", "Block size")
    CHI_OPT_DESC_SIZE(block.chunk, CHI_CHUNK_MIN_SIZE, CHI_GiB(1), "b:c", "Block chunk size")
    CHI_OPT_DESC_UINT32(block.gen, 1, CHI_GEN_MAX, "b:g", "Number of block heap generations")
    CHI_OPT_DESC_TITLE("MAJOR HEAP")
    CHI_OPT_DESC_SIZE(heap.limit.size, CHI_CHUNK_MIN_SIZE, CHI_CHUNK_MAX_SIZE, "h", "Maximal heap size")
    CHI_OPT_DESC_UINT32(heap.limit.soft, 10, 100, "h:ls", "Heap soft limit, Percent of maximal size")
    CHI_OPT_DESC_UINT32(heap.limit.hard, 10, 100, "h:lh", "Heap hard limit, Percent of maximal size")
    CHI_OPT_DESC_UINT32(heap.limit.alloc, 1, 100, "h:la", "Heap allocation limit, Percent of current size")
    CHI_OPT_DESC_SIZE(heap.small.chunk, CHI_MiB(1), CHI_GiB(1), "h:sc", "Heap small chunk size")
    CHI_OPT_DESC_SIZE(heap.small.max, CHI_KiB(1), CHI_MiB(1), "h:so", "Maximal small object size")
    CHI_OPT_DESC_UINT32(heap.small.sub, 1, 2, "h:ss", "Small heap subdivisons")
    CHI_OPT_DESC_SIZE(heap.medium.chunk, CHI_MiB(1), CHI_GiB(1), "h:mc", "Heap medium chunk size")
    CHI_OPT_DESC_SIZE(heap.medium.max, CHI_CHUNK_MIN_SIZE, CHI_MiB(64), "h:mo", "Maximal medium object size")
    CHI_OPT_DESC_UINT32(heap.medium.sub, 1, 32, "h:ms", "Medium heap subdivisons")
    CHI_IF(CHI_HEAP_CHECK_ENABLED, CHI_OPT_DESC_FLAG(heap.check, "h:chk", "Enable heap check"))
    CHI_IF(CHI_HEAP_PROF_ENABLED, CHI_OPT_DESC_UINT32(heap.prof, 0, CHI_GEN_MAX, "h:prof", "Enable heap profile at GC of given gen"))
    CHI_OPT_DESC_UINT32(heap.scanDepth, 0, 10000, "h:d", "Heap scan recursion depth")
    CHI_OPT_DESC_TITLE("GC")
    CHI_OPT_DESC_CHOICE(gc.mode, "gc", "Choose garbage collector", CHI_FOREACH_GCMODE(CHOICE_NAME, ", "))
    CHI_OPT_DESC_UINT64(_CHI_UN(Micros, gc.ms.slice), 100, 1000000, "gc:t", "Time slice in µs of incremental mark & sweep")
    CHI_IF(CHI_GC_CONC_ENABLED, CHI_OPT_DESC_UINT32(gc.conc[0].markers, 0, 100, "gc:mw", "Number of marking workers"))
    CHI_IF(CHI_GC_CONC_ENABLED, CHI_OPT_DESC_UINT32(gc.conc[0].sweepers, 0, 100, "gc:sw", "Number of sweeping workers"))
    CHI_OPT_DESC_FLAG(gc.ms.noCollapse, "gc:nocol", "Disable mark and sweep thunk collapsing")
    CHI_OPT_DESC_UINT32(gc.scav.multiplicity, 2, 50, "scav:m", "Scavenge old generation n times less often")
    CHI_OPT_DESC_FLAG(gc.scav.noCollapse, "scav:nocol", "Disable scavenger thunk collapsing")
    CHI_OPT_DESC_FLAG(gc.scav.noPromote, "scav:nopro", "Disable promotion to major heap")
    CHI_OPT_DESC_UINT64(_CHI_UN(Micros, gc.scav.slice), 100, 100000, "scav:t", "Time slice in µs for scavenger")
#if CHI_EVENT_ENABLED
    CHI_OPT_DESC_TITLE("EVENT")
    CHI_OPT_DESC_CHOICE(event[0].format, "ev", "Write event log in given format",
                        CHI_FOREACH_EVENTFORMAT(EVENTFORMAT_NAME, ", "))
    CHI_OPT_DESC_STRING(event[0].file, "ev:o", "Set log output ("FILE_TYPES")")
    CHI_OPT_DESC_CB(STRING, setEventFilter, "ev:f", "Event filter ("FILTER_EXAMPLES")")
    CHI_OPT_DESC_SIZE(event[0].msgSize, 512, CHI_KiB(64), "ev:m", "Maximal message size")
    CHI_OPT_DESC_UINT32(event[0].bufSize, 0, 1000, "ev:b", "Buffer size in messages")
#endif
#if CHI_STATS_ENABLED
    CHI_OPT_DESC_TITLE("STATISTICS")
    CHI_OPT_DESC(SET, .name = "stats\0Print statistics",
                 .field = CHI_OPT_FIELD(uint32_t, stats[0].enabled), .set = { 1, sizeof (uint32_t) })
    CHI_OPT_DESC(SET, .name = "stats:v\0Verbose output",
                 .field = CHI_OPT_FIELD(uint32_t, stats[0].enabled), .set = { 2, sizeof (uint32_t) })
    CHI_OPT_DESC_FLAG(stats[0].json, "stats:json", "Output as json")
    CHI_OPT_DESC_STRING(stats[0].file, "stats:o", "Set statistics output ("FILE_TYPES")")
    CHI_OPT_DESC_UINT32(stats[0].tableRows, 1, 10000, "stats:tr", "Table rows")
    CHI_OPT_DESC_UINT32(stats[0].tableCell, 1, 1000, "stats:tc", "Table cell size")
    CHI_OPT_DESC_UINT32(stats[0].bins, 1, 1000, "stats:b", "Histogram bins")
    CHI_OPT_DESC_UINT32(stats[0].scale, 110, 1000, "stats:s", "Histogram log scale")
    CHI_OPT_DESC_FLAG(stats[0].cumulative, "stats:c", "Cumulative histogram")
#endif
    CHI_OPT_DESC_END
};
#undef CHI_OPT_TARGET
