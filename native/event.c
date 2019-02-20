#include "runtime.h"
#include "event.h"

#if CHI_EVENT_ENABLED

#include "event/log.h"
#include "sink.h"
#include "mem.h"
#include "error.h"
#include "stats.h"
#include "strutil.h"
#include "thread.h"
#include "color.h"
#include "event/utils.h"
#include <chili/object/string.h>
#include <math.h>

#define HISTOGRAM_SIZE (sizeof (uint64_t) * opt->stats->bins * DSTATS_COUNT)

#define RAWSTR(s)       CHECK(logString, (s))
#define SAFE_QSTR(x)    ({ CHAR('"'); RAWSTR(x); CHAR('"'); })
#define NUM(x)          CHECK(logUNumber, (x))
#define SNUM(x)         CHECK(logSNumber, (x))
#define CHAR(x)         CHECK(logChar, (x))
#define CHECK(fn, ...)  ({ if (!fn(log, ##__VA_ARGS__)) return false; })

typedef enum {
    _CHI_EVENT_DSTATS,
    DSTATS_COUNT,
    DSTATS_NONE,
    DSTATS_GC_SCAVENGER_GEN = DSTATS_GC_SCAVENGER + 1,
} DStatsType;

typedef enum {
    _CHI_EVENT_ISTATS,
    ISTATS_COUNT,
    ISTATS_NONE,
} IStatsType;

typedef struct {
    ChiNanos  max, sum, begin;
    uint64_t  count;
    uint64_t* histogram;
} DStats;

typedef enum {
    CTX_THREAD,
    CTX_PROCESSOR,
    CTX_RUNTIME,
    CTX_WORKER,
} EventContext;

typedef enum {
    CLASS_BEGIN,
    CLASS_END,
    CLASS_INSTANT,
} EventClass;

typedef struct {
    ChiEvent     type;
    uint32_t     wid;
    Chili        thread;
    ChiNanos     time;
    ChiNanos     dur;
    ChiEventData data;
} Event;

typedef struct {
    size_t            blockLastUsedBytes, blockTotalBytes;
    size_t            nurseryMin;
    size_t            smallTotalBytes, mediumTotalBytes, largeTotalBytes;
    uint64_t          blockAllocBytes, smallAllocBytes, mediumAllocBytes, largeAllocBytes;
    ChiObjectCount    totalCopied, totalPromoted;
    _Atomic(uint32_t) threadMaxCount;
} SpecialStats;

typedef struct ChiEventState_ {
    SpecialStats   special;
    ChiMutex       mutex;
    ChiSink*       sink;
    DStats         totalDStats[DSTATS_COUNT];
    uint64_t       totalIStats[ISTATS_COUNT];
    uint32_t       histogramBins;
    float          histogramScale;
    uint64_t       histogramBuffer[0];
} ChiEventState;

typedef struct ChiEventWS_ {
    uint64_t      istats[ISTATS_COUNT];
    DStats        dstats[DSTATS_COUNT];
    uint64_t      histogramBuffer[0];
} ChiEventWS;

_CHI_EVENT_DESC

static CHI_WU bool jsonEscape(Log* log, ChiStringRef s) {
    for (size_t i = 0; i < s.size; ++i) {
        char c = (char)s.bytes[i];
        switch (c) {
        case '"': RAWSTR("\\\""); break;
        case '\n': RAWSTR("\\n"); break;
        case '\\': RAWSTR("\\"); break;
        default:
            if (c > 0x1F) {
                CHAR(c);
            } else {
                RAWSTR("\\x");
                CHAR(chiHexDigit[(c / 16) & 15]);
                CHAR(chiHexDigit[c & 15]);
            }
        }
    }
    return true;
}

// From fastapprox by Paul Mineiro
CHI_INL float fastlog2(float x) {
    union { float f; uint32_t i; } vx = { x };
    union { uint32_t i; float f; } mx = { (vx.i & 0x7FFFFF) | 0x3F000000 };
    float y = (float)vx.i;
    y *= 1.1920928955078125e-7f;
    return y - 124.22551499f
        - 1.498030302f * mx.f
        - 1.72587999f / (.3520887068f + mx.f);
}

static uint32_t nanosToBin(const ChiEventState* state, ChiNanos ns) {
    float t = (float)CHI_UN(Nanos, ns) / 1e4f;
    return t <= 1 ? 0 : CHI_MIN((uint32_t)(fastlog2(t) / state->histogramScale + .5f), state->histogramBins - 1);
}

static ChiNanos binToNanos(const ChiEventState* state, uint32_t bin) {
    return (ChiNanos){(uint64_t)(1e4f * exp2f((float)bin * state->histogramScale))};
}

static void dstatsCollect(ChiEventState* state, DStats* ds, Event* e) {
    DStats* s = ds + eventDesc[e->type].stats;
    if (eventDesc[e->type].cls == CLASS_BEGIN) {
        s->begin = e->time;
        return;
    }

    e->dur = chiNanosDelta(e->time, s->begin);
    e->time = s->begin;
    s->max = chiNanosMax(s->max, e->dur);
    s->sum = chiNanosAdd(s->sum, e->dur);
    ++s->count;
    uint32_t bin = nanosToBin(state, e->dur);
    ++s->histogram[bin];

    if (e->type == CHI_EVENT_GC_SCAVENGER_END) {
        DStats* gs = ds + DSTATS_GC_SCAVENGER_GEN + e->data.GC_SCAVENGER_END->gen;
        gs->max = chiNanosMax(gs->max, e->dur);
        gs->sum = chiNanosAdd(gs->sum, e->dur);
        ++gs->count;
        ++gs->histogram[bin];
    }
}

static void statsMerge(ChiEventState* state, ChiEventWS* pstate) {
    for (DStats *s = state->totalDStats, *ps = pstate->dstats;
         s < state->totalDStats + DSTATS_COUNT; ++s, ++ps) {
        s->max = chiNanosMax(s->max, ps->max);
        s->sum = chiNanosAdd(s->sum, ps->sum);
        s->count += ps->count;
        for (uint32_t i = 0; i < state->histogramBins; ++i)
            s->histogram[i] += ps->histogram[i];
    }

    for (uint32_t i = 0; i < ISTATS_COUNT; ++i)
        state->totalIStats[i] += pstate->istats[i];
}

static void collectSpecialStats(SpecialStats* s, const Event* e) {
    CHI_WARN_OFF(switch-enum)
    switch (e->type) {
    case CHI_EVENT_GC_SCAVENGER_END:
        {
            const ChiEventScavenger* scav = e->data.GC_SCAVENGER_END;
            chiAddObjectCounts(&s->totalCopied, &scav->object.copied1);
            chiAddObjectCounts(&s->totalCopied, &scav->object.copied);
            chiAddObjectCounts(&s->totalCopied, &scav->raw.copied);
            chiAddObjectCounts(&s->totalPromoted, &scav->object.promoted);
            chiAddObjectCounts(&s->totalPromoted, &scav->raw.promoted);
            break;
        }

    case CHI_EVENT_HEAP_BEFORE_SCAV:
    case CHI_EVENT_HEAP_AFTER_SCAV:
        {
            const ChiMinorUsage* minor = &e->data.HEAP_AFTER_SCAV->minor;
            const ChiHeapUsage* major = &e->data.HEAP_AFTER_SCAV->major;
            s->blockTotalBytes = CHI_MAX(s->blockTotalBytes, CHI_WORDSIZE * minor->totalWords);
            if (e->type == CHI_EVENT_HEAP_BEFORE_SCAV &&
                CHI_WORDSIZE * minor->usedWords > s->blockLastUsedBytes)
                s->blockAllocBytes += CHI_WORDSIZE * minor->usedWords - s->blockLastUsedBytes;
            else if (e->type == CHI_EVENT_HEAP_AFTER_SCAV)
                s->blockLastUsedBytes = CHI_WORDSIZE * minor->usedWords;
            s->smallTotalBytes  = CHI_MAX(s->smallTotalBytes,  CHI_WORDSIZE * major->small.totalWords);
            s->mediumTotalBytes = CHI_MAX(s->mediumTotalBytes, CHI_WORDSIZE * major->medium.totalWords);
            s->largeTotalBytes  = CHI_MAX(s->largeTotalBytes,  CHI_WORDSIZE * major->large.totalWords);
            s->smallAllocBytes = CHI_MAX(s->smallAllocBytes, CHI_WORDSIZE * major->small.allocSinceStart);
            s->mediumAllocBytes = CHI_MAX(s->mediumAllocBytes, CHI_WORDSIZE * major->medium.allocSinceStart);
            s->largeAllocBytes = CHI_MAX(s->largeAllocBytes, CHI_WORDSIZE * major->large.allocSinceStart);
            break;
        }

    case CHI_EVENT_NURSERY_RESIZE:
        s->nurseryMin = CHI_MIN(s->nurseryMin, e->data.NURSERY_RESIZE->newLimit);
        break;

    case CHI_EVENT_THREAD_NEW:
        for (;;) {
            uint32_t max = e->data.THREAD_NEW->count, old = s->threadMaxCount;
            if (max <= old || atomic_compare_exchange_weak(&s->threadMaxCount, &old, max))
                break;
        }
        break;

    default:
        break;
    }
    CHI_WARN_ON
}

static ChiNanos computePercentile(const DStats* s, uint32_t percent, const ChiEventState* state) {
    uint64_t sum = 0;
    uint32_t bin = 0;
    for (; bin < state->histogramBins && sum < percent * s->count / 100; ++bin)
        sum += s->histogram[bin];
    return chiNanosMin(s->max, binToNanos(state, bin));
}

static void addStats(ChiRuntime* rt, const char* row, const DStats* s) {
    ChiStats* stats = &rt->stats;
    ChiEventState* state = rt->eventState;
    ChiNanos
        p75 = computePercentile(s, 75, state),
        p90 = computePercentile(s, 90, state),
        mean = (ChiNanos){s->count > 0 ? CHI_UN(Nanos, s->sum) / s->count : 0};
    if (row)
        chiStatsRow(stats, row);
    chiStatsInt(stats, "count", s->count);
    chiStatsTime(stats, "tot", s->sum);
    chiStatsTime(stats, "max", s->max);
    chiStatsTime(stats, "mean", mean);
    chiStatsTime(stats, "p75", p75);
    chiStatsTime(stats, "p90", p90);
}

static void statsGC(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;
    ChiStats* stats = &rt->stats;
    const DStats* s = rt->eventState->totalDStats;
    const SpecialStats* p = &rt->eventState->special;

    chiStatsTitle(stats, "gc");

    addStats(rt, "sync",      s + DSTATS_PROC_SYNC);
    addStats(rt, "wait sync", s + DSTATS_PROC_WAIT_SYNC);
    addStats(rt, "gc slice",  s + DSTATS_GC_SLICE);

    if (opt->gc.mode != CHI_GC_NOMS) {
        chiStatsRow(stats, "mark sweep");
#define _CHI_GCMODE(n, s) CHI_STRINGIZE(s),
        static const char* const gcMode[] = { CHI_FOREACH_GCMODE(_CHI_GCMODE,) };
#undef _CHI_GCMODE
        chiStatsString(stats, "mode", chiStringRef(gcMode[opt->gc.mode]));
        addStats(rt, 0, s + DSTATS_GC_MARKSWEEP);

        if (opt->stats->enabled > 1) {
            if (CHI_AND(CHI_GC_CONC_ENABLED, opt->gc.mode == CHI_GC_CONC)) {
                chiStatsRow(stats, "workers");
                chiStatsInt(stats, "markers", opt->gc.conc->markers);
                chiStatsInt(stats, "sweepers", opt->gc.conc->sweepers);
            }

            addStats(rt, "m slice", s + DSTATS_GC_MARK_SLICE);
            addStats(rt, "s slice", s + DSTATS_GC_SWEEP_SLICE);
            addStats(rt, "m phase", s + DSTATS_GC_MARK_PHASE);
            addStats(rt, "s phase", s + DSTATS_GC_SWEEP_PHASE);
        }
    }

    addStats(rt, "scavenger", s + DSTATS_GC_SCAVENGER);
    if (!chiMicrosZero(opt->gc.scav.slice))
        chiStatsTime(stats, "slice", chiMicrosToNanos(opt->gc.scav.slice));

    if (opt->stats->enabled > 1) {
        for (uint32_t gen = 0; gen < opt->block.gen; ++gen) {
            char genName[6];
            chiFmt(genName, sizeof (genName), "gen%u", gen);
            addStats(rt, genName, s + DSTATS_GC_SCAVENGER_GEN + gen);
        }

        chiStatsRow(stats, "copied");
        chiStatsBytes(stats, "size", CHI_WORDSIZE * p->totalCopied.words);
        chiStatsInt(stats, "objects", p->totalCopied.count);
        chiStatsBytesPerSec(stats, "rate", chiPerSec(CHI_WORDSIZE * p->totalCopied.words, s[DSTATS_GC_SCAVENGER].sum));

        chiStatsRow(stats, "promoted");
        chiStatsBytes(stats, "size", CHI_WORDSIZE * p->totalPromoted.words);
        chiStatsInt(stats, "objects", p->totalPromoted.count);
        chiStatsBytesPerSec(stats, "rate", chiPerSec(CHI_WORDSIZE * p->totalPromoted.words, s[DSTATS_GC_SCAVENGER].sum));
    }
}

static void statsMem(ChiRuntime* rt, const ChiTime delta) {
    const DStats* s = rt->eventState->totalDStats;
    const ChiRuntimeOption* opt = &rt->option;
    ChiStats* stats = &rt->stats;
    const SpecialStats* p = &rt->eventState->special;

    ChiActivity activity;
    chiActivity(&activity);

    chiStatsTitle(stats, "memory");

    chiStatsRow(stats, "mem usage");
    chiStatsBytes(stats, "heaps", p->smallTotalBytes + p->mediumTotalBytes + p->largeTotalBytes + p->blockTotalBytes);
    chiStatsBytes(stats, "resident", activity.residentSize);

    chiStatsRow(stats, "major heap");
    chiStatsBytes(stats, "size", p->smallTotalBytes + p->mediumTotalBytes + p->largeTotalBytes);
    chiStatsBytes(stats, "alloc", p->smallAllocBytes + p->mediumAllocBytes + p->largeAllocBytes);

    if (opt->stats->enabled > 1) {
        chiStatsRow(stats, "small");
        chiStatsBytes(stats, "chunk", opt->heap.small.chunk);
        chiStatsBytes(stats, "max", opt->heap.small.max);
        chiStatsInt(stats, "sub", opt->heap.small.sub);
        chiStatsBytes(stats, "size", p->smallTotalBytes);
        chiStatsBytes(stats, "alloc", p->smallAllocBytes);

        chiStatsRow(stats, "medium");
        chiStatsBytes(stats, "chunk", opt->heap.medium.chunk);
        chiStatsBytes(stats, "max", opt->heap.medium.max);
        chiStatsInt(stats, "sub", opt->heap.medium.sub);
        chiStatsBytes(stats, "size", p->mediumTotalBytes);
        chiStatsBytes(stats, "alloc", p->mediumAllocBytes);

        chiStatsRow(stats, "large");
        chiStatsBytes(stats, "size", p->largeTotalBytes);
        chiStatsBytes(stats, "alloc", p->largeAllocBytes);
    }

    chiStatsRow(stats, "minor heap");
    chiStatsBytes(stats, "size", p->blockTotalBytes);
    chiStatsBytes(stats, "block", opt->block.size);
    chiStatsBytes(stats, "nursery", opt->block.size * opt->block.nursery);
    if (!chiMicrosZero(opt->gc.scav.slice))
        chiStatsBytes(stats, "min", opt->block.size * p->nurseryMin);

    chiStatsRow(stats, "mutator");
    chiStatsBytes(stats, "alloc", p->blockAllocBytes);
    chiStatsBytesPerSec(stats, "rate", chiPerSec(p->blockAllocBytes, chiNanosDelta(delta.real, s[DSTATS_GC_SLICE].sum)));
    chiStatsBytesPerSec(stats, "rate/cpu",
                         chiPerSec(p->blockAllocBytes,
                                         chiNanosAdd(s[DSTATS_THREAD_RUN].sum, s[DSTATS_THREAD_SCHED].sum)));

    if (s[DSTATS_HEAP_CHECK].count)
        addStats(rt, "heap check", s + DSTATS_HEAP_CHECK);

    if (s[DSTATS_HEAP_DUMP].count)
        addStats(rt, "heap dump", s + DSTATS_HEAP_DUMP);

    if (s[DSTATS_HEAP_PROF].count)
        addStats(rt, "heap prof", s + DSTATS_HEAP_PROF);
}

static void statsRuntime(ChiRuntime* rt, const ChiTime delta) {
    const ChiEventState* state = rt->eventState;
    ChiStats* stats = &rt->stats;
    const DStats* s = state->totalDStats;
    const uint64_t* is = state->totalIStats;
    const SpecialStats* p = &state->special;

    chiStatsTitle(stats, "runtime");

    chiStatsRow(stats, "runtime");
    chiStatsInt(stats, "processors", is[ISTATS_PROC_INIT]);
    chiStatsTime(stats,  "real", delta.real);
    chiStatsTime(stats,  "cpu", delta.cpu);
    chiStatsTime(stats,  "startup", s[DSTATS_STARTUP].sum);
    chiStatsTime(stats,  "shutdown", s[DSTATS_SHUTDOWN].sum);

    chiStatsRow(stats, "productive");
    chiStatsPercent(stats, "usage", chiNanosRatio(delta.cpu, delta.real));
    chiStatsPercent(stats, "load", chiNanosRatio(chiNanosAdd(s[DSTATS_PROC_RUN].sum, s[DSTATS_GC_SLICE].sum), delta.real));

    ChiNanos total = chiNanosAdd(s[DSTATS_PROC_RUN].sum, s[DSTATS_PROC_SYNC].sum);
    chiStatsPercent(stats, "mutator", chiNanosRatio(s[DSTATS_THREAD_RUN].sum, total));
    chiStatsPercent(stats, "scheduler", chiNanosRatio(s[DSTATS_THREAD_SCHED].sum, total));
    chiStatsPercent(stats, "gc", chiNanosRatio(s[DSTATS_PROC_SYNC].sum, total));

    addStats(rt, "mutator", s + DSTATS_THREAD_RUN);
    addStats(rt, "scheduler", s + DSTATS_THREAD_SCHED);
    addStats(rt, "suspended", s + DSTATS_PROC_SUSPEND);

    chiStatsRow(stats, "threads");
    chiStatsInt(stats, "new", is[ISTATS_THREAD_NEW]);
    chiStatsInt(stats, "term", is[ISTATS_THREAD_TERMINATED]);
    chiStatsInt(stats, "max", p->threadMaxCount);
    chiStatsInt(stats, "except", is[ISTATS_EXCEPTION_HANDLED] + is[ISTATS_EXCEPTION_UNHANDLED]);
    chiStatsInt(stats, "par", is[ISTATS_PAR]);
    chiStatsInt(stats, "grow", is[ISTATS_STACK_GROW]);
    chiStatsInt(stats, "shrink", is[ISTATS_STACK_GROW]);
}

typedef struct {
    size_t      id;
    const char* header;
} HistogramColumn;

static ChiStatsColumn* statsHistogramColumn(ChiStatsColumn* col, const char* header, const DStats* s, uint32_t rows, uint32_t offset, bool cumulative) {
    uint64_t* histogram = chiAllocArr(uint64_t, rows);
    for (uint32_t j = 0; j < rows; ++j)
        histogram[j] = (cumulative && j > 0 ? histogram[j - 1] : 0) + s->histogram[j + offset];
    *col++ = (ChiStatsColumn){ .header = header, .type = CHI_STATS_INT, .ints = histogram, .sum = s->count, .percent = true, .cumulative = cumulative };
    *col++ = (ChiStatsColumn){ .header = "n", .type = CHI_STATS_INT, .ints = histogram, .sep = true };
    return col;
}

static bool statsHistogramHideRow(const DStats* s, uint32_t row, uint32_t ntcols, const HistogramColumn* tcol) {
    for (uint32_t i = 0; i < ntcols; ++i) {
        if (s[tcol[i].id].histogram[row])
            return false;
    }
    return true;
}

static void statsHistogram(ChiRuntime* rt, const char* title, uint32_t ntcols, const HistogramColumn* tcol) {
    const ChiEventState* state = rt->eventState;
    const DStats* s = state->totalDStats;
    ChiStats* stats = &rt->stats;
    bool cumulative = rt->option.stats->cumulative;

    CHI_AUTO_ALLOC(ChiStatsColumn, column, 1 + 2 * ntcols);
    ChiStatsColumn* col = column;
    uint32_t offset = 0;
    for (; offset < state->histogramBins && statsHistogramHideRow(s, offset, ntcols, tcol); ++offset) {}

    uint32_t rows = state->histogramBins - offset;
    for (; rows > 0 && statsHistogramHideRow(s, offset + rows - 1, ntcols, tcol); --rows) {}

    uint64_t* durationCol = chiAllocArr(uint64_t, rows);
    for (uint32_t j = 0; j < rows; ++j)
        durationCol[j] = CHI_UN(Micros, chiNanosToMicros(binToNanos(state, j + offset)));
    *col++ = (ChiStatsColumn){ .header = "usec", .type = CHI_STATS_INT, .ints = durationCol, .sep = true };

    for (uint32_t i = 0; i < ntcols; ++i)
        col = statsHistogramColumn(col, tcol[i].header, s + tcol[i].id, rows, offset, cumulative);

    char buf[64];
    chiFmt(buf, sizeof (buf), "%s%s histogram", title, cumulative ? " cumulative" : "");
    chiStatsTable(stats, .title = buf, .rows = rows, .columns = (uint32_t)(col - column), .data = column);
}

static void statsHistogramTables(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;

    if (opt->gc.mode != CHI_GC_NONE) {
        const uint32_t blockGen = opt->block.gen;
        HistogramColumn scav[1 + CHI_GEN_MAX] = { { DSTATS_GC_SLICE, "slice" } };
        char genHeader[CHI_GEN_MAX][6];
        for (uint32_t i = 0; i < blockGen; ++i) {
            chiFmt(genHeader[i], sizeof (genHeader[i]), "gen%u", i);
            scav[i + 1].header = genHeader[i];
            scav[i + 1].id = DSTATS_GC_SCAVENGER_GEN + i;
        }
        statsHistogram(rt, "scavenger", 1 + blockGen, scav);

        if (opt->gc.mode != CHI_GC_NOMS) {
            HistogramColumn ms[] = { { DSTATS_GC_MARK_SLICE, "m slice" }, { DSTATS_GC_SWEEP_SLICE, "s slice" } };
            statsHistogram(rt, "mark & sweep", CHI_DIM(ms), ms);
        }
    }

    HistogramColumn thread[] = { { DSTATS_THREAD_RUN, "thread" }, { DSTATS_THREAD_SCHED, "sched" } };
    statsHistogram(rt, "thread", CHI_DIM(thread), thread);

    HistogramColumn sync[] = { { DSTATS_PROC_SYNC, "sync" }, { DSTATS_PROC_WAIT_SYNC, "wait" } };
    statsHistogram(rt, "sync", CHI_DIM(sync), sync);
}

static void statsOutput(ChiRuntime* rt) {
    const ChiEventState* state = rt->eventState;
    const ChiRuntimeOption* opt = &rt->option;
    if (state && opt->stats->enabled) {
        ChiTime delta = chiTimeDelta(rt->timeRef.end, rt->timeRef.start);
        statsRuntime(rt, delta);
        statsMem(rt, delta);
        if (opt->gc.mode != CHI_GC_NONE)
            statsGC(rt);
        if (opt->stats->enabled > 1)
            statsHistogramTables(rt);
    }
}

static Log* logLocal(size_t msgSize) {
    static _Thread_local Log* log = 0;
    if (!log)
        log = (Log*)chiTaskLocal(msgSize, 0);
    logInit(log, msgSize - sizeof (Log));
    return log;
}

static void logCommit(ChiEventState* state, Log* log, bool ret) {
    ChiStringRef s = { .bytes = log->buf, .size = (uint32_t)(log->ptr - log->buf) };
    if (ret)
        chiSinkPuts(state->sink, s);
    else
        chiWarn("Event message too long. Discarded message:\n%S...", s);
}

#include "event/pretty.h"
#include "event/writer.h"
#include "event/json.h"
#include "event/writer.h"
#include "event/msgpack.h"
#include "event/writer.h"
#include "event/csv.h"
#include "event/writer.h"
#include "event/xml.h"
#include "event/writer.h"
#include "event/te.h"

static CHI_WU bool noneEvent(Log* CHI_UNUSED(log), const Event* CHI_UNUSED(e)) {
    return true;
}

void chiEvent(void* ctx, ChiEvent e, ChiEventData d) {
    CHI_ASSERT(ctx);

    ChiWorker* worker;
    ChiRuntime* rt;
    if (eventDesc[e].ctx == CTX_THREAD
        || eventDesc[e].ctx == CTX_PROCESSOR
        || eventDesc[e].ctx == CTX_WORKER) {
        worker = (ChiWorker*)ctx;
        rt = worker->rt;
    } else {
        rt = (ChiRuntime*)ctx;
        worker = 0;
    }

    const ChiRuntimeOption* opt = &rt->option;
    CHI_ASSERT(e == CHI_EVENT_BEGIN || e == CHI_EVENT_END || chiBitGet(opt->event->filter, e));

    ChiEventState* state = rt->eventState;
    if (!state)
        return;

    Event event = {
        .type = e,
        .data = d,
        .time = chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), rt->timeRef.start.real),
        .wid = worker ? worker->wid : 0,
        .thread = eventDesc[e].ctx == CTX_THREAD ? chiWorkerToProcessor(worker)->thread : CHI_FALSE
    };

    if (eventDesc[e].cls == CLASS_INSTANT) {
        if (eventDesc[e].stats != ISTATS_NONE)
            ++eventDesc[e].stats[worker ? worker->eventWS->istats : state->totalIStats];
    } else {
        if (eventDesc[e].stats != DSTATS_NONE)
            dstatsCollect(state, worker ? worker->eventWS->dstats : state->totalDStats, &event);
    }
    collectSpecialStats(&state->special, &event);

    if (eventDesc[e].cls != CLASS_BEGIN && opt->event->format != CHI_EVENTFORMAT_NONE) {
        Log* log = logLocal(opt->event->msgSize);

        CHI_NOWARN_UNUSED(teEvent);
        CHI_NOWARN_UNUSED(jsonEvent);
        CHI_NOWARN_UNUSED(csvEvent);
        CHI_NOWARN_UNUSED(xmlEvent);
        CHI_NOWARN_UNUSED(mpEvent);
        CHI_NOWARN_UNUSED(prettyEvent);

        bool ok;
        switch (opt->event->format) {
#define SELECT_EVENTFORMAT(N, n, e) case CHI_EVENTFORMAT_##N: ok = n##Event(log, &event); break;
        CHI_FOREACH_EVENTFORMAT(SELECT_EVENTFORMAT,)
#undef SELECT_EVENTFORMAT
        default: CHI_BUG("Invalid event format");
        }

        logCommit(state, log, ok);
    }
}

void chiEventWorkerStart(ChiWorker* worker) {
    ChiEventState* state = worker->rt->eventState;
    if (!state)
        return;

    const ChiRuntimeOption* opt = &worker->rt->option;
    worker->eventWS = (ChiEventWS*)chiZalloc(sizeof (ChiEventWS) + HISTOGRAM_SIZE);
    for (uint32_t i = 0; i < DSTATS_COUNT; ++i)
        worker->eventWS->dstats[i].histogram = worker->eventWS->histogramBuffer + i * state->histogramBins;

    CHI_EVENT(worker, WORKER_NAME, .name = chiStringRef(worker->name));
    CHI_EVENT0(worker, WORKER_INIT);
}

void chiEventWorkerStop(ChiWorker* worker) {
    if (worker->eventWS) {
        CHI_EVENT0(worker, WORKER_DESTROY);
        ChiEventState* state = worker->rt->eventState;
        {
            CHI_LOCK(&state->mutex);
            statsMerge(state, worker->eventWS);
        }
        chiFree(worker->eventWS);
    }
}

void chiEventDestroy(ChiRuntime* rt) {
    ChiEventState* state = rt->eventState;
    if (!state)
        return;
    CHI_EVENT0(rt, SHUTDOWN_END);

    if (state->sink) {
        // Always write the end event to ensure proper log finalization (in particular for json)
        chiEvent(rt, CHI_EVENT_END, (ChiEventData){0});
        chiSinkClose(state->sink);
    }

    statsOutput(rt);
    chiMutexDestroy(&state->mutex);
    chiFree(state);
    rt->eventState = 0;
}

void chiEventSetup(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;

    if (!opt->stats->enabled && !opt->event->format) {
        CHI_CLEAR_ARRAY(rt->option.event->filter);
        return;
    }

    ChiSink* sink = 0;
    if (opt->event->format != CHI_EVENTFORMAT_NONE) {
        ChiSinkColor color =
            CHI_AND(CHI_EVENT_PRETTY_ENABLED, opt->event->format == CHI_EVENTFORMAT_PRETTY)
            ? opt->color : CHI_SINK_BINARY;
        sink = chiSinkLockNew(chiSinkFileTryNew(opt->event->file, color),
                              opt->event->bufSize * opt->event->msgSize);
        if (!sink) {
            CHI_CLEAR_ARRAY(rt->option.event->filter);
            return;
        }
    }

    ChiEventState* state = rt->eventState = (ChiEventState*)chiZalloc(sizeof (ChiEventState) + HISTOGRAM_SIZE);
    chiMutexInit(&state->mutex);
    state->special.nurseryMin = opt->block.nursery;
    state->sink = sink;
    state->histogramBins = opt->stats->bins;
    state->histogramScale = fastlog2((float)opt->stats->scale / 100.f);
    for (uint32_t i = 0; i < DSTATS_COUNT; ++i)
        state->totalDStats[i].histogram = state->histogramBuffer + i * state->histogramBins;

    // Always write the begin event to ensure proper log initialization (in particular for json)
    chiEvent(rt, CHI_EVENT_BEGIN, (ChiEventData){.BEGIN=&(ChiEventVersion){ .version = CHI_EVENT_VERSION }});
    CHI_EVENT0(rt, STARTUP_BEGIN);
}

bool chiEventModifyFilter(uint8_t* filter, ChiStringRef s) {
    uint8_t f[CHI_EVENT_FILTER_SIZE];
    memcpy(f, filter, sizeof (f));

    const uint8_t* p = s.bytes, *end = p + s.size;
    while (p < end) {
        bool overwrite = *p != '-' && *p != '+';
        bool set = overwrite || *p++ == '+';
        if (overwrite)
            memset(f, 0, sizeof (f));

        size_t len = (size_t)(end - p);
        const uint8_t
            *nextPlus = (uint8_t*)memchr(p, '+', len),
            *nextMinus = (uint8_t*)memchr(p, '-', len),
            *next =
            nextPlus && nextMinus ? CHI_MIN(nextPlus, nextMinus) :
            nextPlus ? nextPlus :
            nextMinus ? nextMinus : end;

        len = (size_t)(next - p);
        if (!len) {
            chiWarn("Invalid empty event filter pattern");
            return false;
        }

        if (memeqstr(p, len, "ALL")) {
            memset(f, set ? 0xFF : 0, sizeof (f));
        } else {
            bool found = false;
            for (size_t i = 0; i < _CHI_EVENT_COUNT; ++i) {
                if (!strncmp(chiEventName[i], (const char*)p, len)) {
                    chiBitSet(f, i, set);
                    found = true;
                }
            }
            if (!found) {
                chiWarn("Invalid event filter pattern '%b'", (uint32_t)len, p);
                return false;
            }
        }
        p = next;
    }

    memcpy(filter, f, sizeof (f));
    return true;
}

#if CHI_LTTNG_ENABLED
#  define TRACEPOINT_CREATE_PROBES
#  define TRACEPOINT_DEFINE
#  define TRACEPOINT_HEADER_MULTI_READ
#  include "event/lttng.h"
#endif

#endif

bool chiEventEnabled(void) {
    return CHI_EVENT_P(CHI_CURRENT_PROCESSOR, USER) ||
        CHI_EVENT_P(CHI_CURRENT_PROCESSOR, USER_DURATION_END);
}

void chiEventBegin(void) {
    CHI_EVENT0(CHI_CURRENT_PROCESSOR, USER_DURATION_BEGIN);
}

void chiEventEnd(Chili data) {
    CHI_EVENT(CHI_CURRENT_PROCESSOR, USER_DURATION_END,
              .data = chiStringRef(&data));
}

void chiEventInstant(Chili data) {
    CHI_EVENT(CHI_CURRENT_PROCESSOR, USER,
              .data = chiStringRef(&data));
}

bool chiEventFilter(Chili s) {
    return chiEventModifyFilter(CHI_CURRENT_RUNTIME->option.event->filter, chiStringRef(&s));
}
