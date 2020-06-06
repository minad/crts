typedef struct {
    ChiNanos  max, sum;
    uint64_t* histogram;
} DStats;

typedef struct {
    struct {
        size_t     lastUsedSize, totalSize;
        uint64_t   allocSize;
    } minor;
    struct {
        struct {
            size_t totalSize;
            uint64_t allocSize;
        } small, medium, large;
    } major;
    struct {
        ChiObjectCount totalCopied, totalPromoted, totalDirty;
    } scav;
    size_t stackMaxSize;
} SpecialStats;

typedef struct {
    uint32_t       bins;
    float          scale;
} HistogramDesc;

struct RuntimeStats_ {
    ChiMutex       mutex;
    SpecialStats   special;
    _Atomic(uint32_t) threadCount, threadMaxCount;
    DStats         dstats[DSTATS_COUNT];
    uint64_t       istats[ISTATS_COUNT];
    HistogramDesc  histogramDesc;
    uint64_t       histogramBuffer[0];
};

struct WorkerStats_ {
    SpecialStats   special;
    uint64_t       istats[ISTATS_COUNT];
    DStats         dstats[DSTATS_COUNT];
    uint64_t       histogramBuffer[0];
};

// From fastapprox by Paul Mineiro
static float fastlog2(float x) {
    // TODO: Check big endian
    union { float f; uint32_t i; } v = { .f = x }, m = { .i = (v.i & 0x7FFFFF) | 0x3F000000 };
    float y = (float)v.i;
    y *= 1.1920928955078125e-7f;
    return y - 124.22551499f
        - 1.498030302f * m.f
        - 1.72587999f / (.3520887068f + m.f);
}

static uint32_t nanosToBin(const HistogramDesc* h, ChiNanos ns) {
    float t = (float)CHI_UN(Nanos, ns) / 1e4f;
    return t <= 1 ? 0 : CHI_MIN((uint32_t)(fastlog2(t) / h->scale + .5f), h->bins - 1);
}

static ChiNanos binToNanos(const HistogramDesc* h, uint32_t bin) {
    return chiNanos((uint64_t)(1e4f * exp2f((float)bin * h->scale)));
}

static void dstatsCollect(const HistogramDesc* h, DStats* ds, const Event* e) {
    DStats* s = ds + eventDesc[e->type].stats - 1;
    uint32_t bin = nanosToBin(h, e->dur);
    s->max = chiNanosMax(s->max, e->dur);
    s->sum = chiNanosAdd(s->sum, e->dur);
    ++s->histogram[bin];
}

static void specialStatsCollect(RuntimeStats* stats, SpecialStats* s, const Event* e) {
    CHI_WARN_OFF(switch-enum)
    switch (e->type) {
    case CHI_EVENT_GC_SCAVENGER_END:
        {
            const ChiEventScavenger* d = &e->payload->GC_SCAVENGER_END;
            chiCountAdd(s->scav.totalCopied, d->scavenger.object.copied);
            chiCountAdd(s->scav.totalCopied, d->scavenger.raw.copied);
            chiCountAdd(s->scav.totalPromoted, d->scavenger.object.promoted);
            chiCountAdd(s->scav.totalPromoted, d->scavenger.raw.promoted);
            chiCountAdd(s->scav.totalDirty, d->scavenger.dirty.stack);
            chiCountAdd(s->scav.totalDirty, d->scavenger.dirty.object);
            chiCountAdd(s->scav.totalDirty, d->scavenger.dirty.card);

            CHI_SETMAX(&s->minor.totalSize, d->minorHeapBefore.totalSize);
            CHI_SETMAX(&s->minor.totalSize, d->minorHeapAfter.totalSize);
            if (d->minorHeapBefore.usedSize > s->minor.lastUsedSize)
                s->minor.allocSize += d->minorHeapBefore.usedSize - s->minor.lastUsedSize;
            s->minor.lastUsedSize = d->minorHeapAfter.usedSize;
            break;
        }

    case CHI_EVENT_HEAP_USAGE:
        {
            const ChiHeapUsage* u = &e->payload->HEAP_USAGE;
            CHI_SETMAX(&s->major.small.totalSize,  u->small.totalSize);
            CHI_SETMAX(&s->major.medium.totalSize, u->medium.totalSize);
            CHI_SETMAX(&s->major.large.totalSize,  u->large.totalSize);
            CHI_SETMAX(&s->major.small.allocSize, u->small.allocSize);
            CHI_SETMAX(&s->major.medium.allocSize, u->medium.allocSize);
            CHI_SETMAX(&s->major.large.allocSize, u->large.allocSize);
            break;
        }

    case CHI_EVENT_THREAD_NEW:
        {
            uint32_t count = atomic_fetch_add_explicit(&stats->threadCount, 1, memory_order_relaxed) + 1;
            if (count > atomic_load_explicit(&stats->threadMaxCount, memory_order_relaxed))
                atomic_store_explicit(&stats->threadMaxCount, count, memory_order_relaxed);
            break;
        }

    case CHI_EVENT_THREAD_TERMINATED:
        atomic_fetch_sub_explicit(&stats->threadCount, 1, memory_order_relaxed);
        break;

    case CHI_EVENT_STACK_GROW:
        CHI_SETMAX(&s->stackMaxSize, e->payload->STACK_GROW.size);
        break;

    default:
        break;
    }
    CHI_WARN_ON
}

static void specialStatsMerge(SpecialStats* stats, const SpecialStats* wstats) {
    CHI_SETMAX(&stats->stackMaxSize, wstats->stackMaxSize);
    CHI_SETMAX(&stats->minor.totalSize, wstats->minor.totalSize);
    CHI_SETMAX(&stats->major.small.totalSize, wstats->major.small.totalSize);
    CHI_SETMAX(&stats->major.medium.totalSize, wstats->major.medium.totalSize);
    CHI_SETMAX(&stats->major.large.totalSize, wstats->major.large.totalSize);
    stats->minor.allocSize += wstats->minor.allocSize;
    stats->major.small.allocSize += wstats->major.small.allocSize;
    stats->major.medium.allocSize += wstats->major.medium.allocSize;
    stats->major.large.allocSize += wstats->major.large.allocSize;
    chiCountAdd(stats->scav.totalCopied, wstats->scav.totalCopied);
    chiCountAdd(stats->scav.totalPromoted, wstats->scav.totalPromoted);
    chiCountAdd(stats->scav.totalDirty, wstats->scav.totalDirty);
}

static void statsCollect(RuntimeStats* stats, WorkerStats* wstats, const Event* e) {
    if (!stats)
        return;
    if (eventDesc[e->type].stats) {
        if (eventDesc[e->type].cls == CLASS_INSTANT)
            ++(wstats ? wstats->istats : stats->istats)[eventDesc[e->type].stats - 1];
        else if (eventDesc[e->type].cls == CLASS_END)
            dstatsCollect(&stats->histogramDesc, wstats ? wstats->dstats : stats->dstats, e);
    }
    specialStatsCollect(stats, wstats ? &wstats->special : &stats->special, e);
}

static void statsMerge(RuntimeStats* stats, const WorkerStats* wstats) {
    CHI_LOCK_MUTEX(&stats->mutex);

    const DStats* ws = wstats->dstats;
    for (DStats *s = stats->dstats; s < stats->dstats + DSTATS_COUNT; ++s, ++ws) {
        s->max = chiNanosMax(s->max, ws->max);
        s->sum = chiNanosAdd(s->sum, ws->sum);
        for (uint32_t i = 0; i < stats->histogramDesc.bins; ++i)
            s->histogram[i] += ws->histogram[i];
    }

    for (uint32_t i = 0; i < ISTATS_COUNT; ++i)
        stats->istats[i] += wstats->istats[i];

    specialStatsMerge(&stats->special, &wstats->special);
}

static void histogramInit(DStats* dstats, uint64_t* buf, uint32_t bins) {
    for (uint32_t i = 0; i < DSTATS_COUNT; ++i)
        dstats[i].histogram = buf + i * bins;
}

static WorkerStats* statsWorkerStart(const RuntimeStats* stats) {
    if (!stats)
        return 0;
    WorkerStats* wstats = (WorkerStats*)chiZalloc(sizeof (WorkerStats) + (DSTATS_COUNT * sizeof (uint64_t) * stats->histogramDesc.bins));
    histogramInit(wstats->dstats, wstats->histogramBuffer, stats->histogramDesc.bins);
    return wstats;
}

static void statsWorkerStop(RuntimeStats* stats, WorkerStats* wstats) {
    if (!wstats)
        return;
    statsMerge(stats, wstats);
    chiFree(wstats);
}

static uint64_t dstatsCount(const DStats* s, uint32_t bins) {
    uint64_t count = 0;
    for (uint32_t bin = 0; bin < bins; ++bin)
        count += s->histogram[bin];
    return count;
}

static void statsAdd(ChiRuntime* rt, const char* row, const DStats* s) {
    ChiStats* out = &rt->stats;
    const HistogramDesc* h = &rt->eventState->stats->histogramDesc;
    uint64_t count = dstatsCount(s, h->bins);
    ChiNanos mean = chiNanos(count > 0 ? CHI_UN(Nanos, s->sum) / count : 0);
    ChiNanos delta = chiNanosDelta(rt->timeRef.end, rt->timeRef.start);
    if (row)
        chiStatsRow(out, row);
    chiStatsInt(out, "count", count);
    chiStatsPerSec(out, "rate", chiPerSec(count, delta));
    chiStatsPercent(out, "usage", chiNanosRatio(s->sum, delta));
    chiStatsTime(out, "total", s->sum);
    chiStatsTime(out, "max", s->max);
    chiStatsTime(out, "mean", mean);
}

static void statsGC(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;
    ChiStats* out = &rt->stats;
    const DStats* s = rt->eventState->stats->dstats;
    const SpecialStats* special = &rt->eventState->stats->special;

    chiStatsTitle(out, "gc");

    if (opt->gc.mode != CHI_GC_NONE) {
        chiStatsRow(out, "mark sweep");

#define _CHI_GCMODE(n, s) CHI_STRINGIZE(s),
        static const char* const gcMode[] = { CHI_FOREACH_GCMODE(_CHI_GCMODE,) };
#undef _CHI_GCMODE
        chiStatsString(out, "mode", chiStringRef(gcMode[opt->gc.mode]));
        if (CHI_AND(CHI_GC_CONC_ENABLED, opt->gc.mode == CHI_GC_CONC))
            chiStatsInt(out, "workers", CHI_AND(CHI_GC_CONC_ENABLED, opt->gc.worker));
        statsAdd(rt, 0, s + DSTATS_GC_MARK_PHASE);
        statsAdd(rt, "m slice", s + DSTATS_GC_MARK_SLICE);
        statsAdd(rt, "s slice", s + DSTATS_GC_SWEEP_SLICE);
    }

    statsAdd(rt, "scavenger", s + DSTATS_GC_SCAVENGER);
    chiStatsRow(out, "copied");
    chiStatsBytes(out, "size", CHI_WORDSIZE * special->scav.totalCopied.words);
    chiStatsInt(out, "objects", special->scav.totalCopied.count);
    chiStatsBytesPerSec(out, "rate", chiPerSec(CHI_WORDSIZE * special->scav.totalCopied.words, s[DSTATS_GC_SCAVENGER].sum));

    chiStatsRow(out, "promoted");
    chiStatsBytes(out, "size", CHI_WORDSIZE * special->scav.totalPromoted.words);
    chiStatsInt(out, "objects", special->scav.totalPromoted.count);
    chiStatsBytesPerSec(out, "rate", chiPerSec(CHI_WORDSIZE * special->scav.totalPromoted.words, s[DSTATS_GC_SCAVENGER].sum));

    chiStatsRow(out, "dirty");
    chiStatsBytes(out, "size", CHI_WORDSIZE * special->scav.totalDirty.words);
    chiStatsInt(out, "objects", special->scav.totalDirty.count);
    chiStatsBytesPerSec(out, "rate", chiPerSec(CHI_WORDSIZE * special->scav.totalDirty.words, s[DSTATS_GC_SCAVENGER].sum));
}

static void statsHeaps(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;
    ChiStats* out = &rt->stats;
    const RuntimeStats* stats = rt->eventState->stats;
    const SpecialStats* special = &stats->special;
    ChiNanos delta = chiNanosDelta(rt->timeRef.end, rt->timeRef.start);
    uint64_t majorAlloc = special->major.small.allocSize + special->major.medium.allocSize + special->major.large.allocSize;
    size_t majorSize = special->major.small.totalSize + special->major.medium.totalSize + special->major.large.totalSize;

    chiStatsTitle(out, "heaps");

    chiStatsRow(out, "major heap");
    chiStatsBytes(out, "size", majorSize);
    chiStatsBytes(out, "alloc", majorAlloc);
    chiStatsBytesPerSec(out, "rate", chiPerSec(majorAlloc, delta));

    chiStatsRow(out, "small");
    chiStatsBytes(out, "segment", opt->heap.small.segment);
    chiStatsBytes(out, "page", opt->heap.small.page);
    chiStatsBytes(out, "size", special->major.small.totalSize);
    chiStatsBytes(out, "alloc", special->major.small.allocSize);
    chiStatsBytesPerSec(out, "rate", chiPerSec(special->major.small.allocSize, delta));

    chiStatsRow(out, "medium");
    chiStatsBytes(out, "segment", opt->heap.medium.segment);
    chiStatsBytes(out, "page", opt->heap.medium.page);
    chiStatsBytes(out, "size", special->major.medium.totalSize);
    chiStatsBytes(out, "alloc", special->major.medium.allocSize);
    chiStatsBytesPerSec(out, "rate", chiPerSec(special->major.medium.allocSize, delta));

    chiStatsRow(out, "large");
    chiStatsBytes(out, "size", special->major.large.totalSize);
    chiStatsBytes(out, "alloc", special->major.large.allocSize);
    chiStatsBytesPerSec(out, "rate", chiPerSec(special->major.large.allocSize, delta));

    chiStatsRow(out, "minor heap");
    chiStatsBytes(out, "nursery", opt->nursery.limit);
    chiStatsBytes(out, "block", opt->nursery.block);
    chiStatsBytes(out, "chunk", opt->nursery.chunk);
    chiStatsInt(out, "aging", opt->gc.aging);
    chiStatsBytes(out, "size", special->minor.totalSize);
    chiStatsBytes(out, "alloc", special->minor.allocSize);
    chiStatsBytesPerSec(out, "rate", chiPerSec(special->minor.allocSize, delta));
}

static void statsOverview(ChiRuntime* rt) {
    ChiNanos delta = chiNanosDelta(rt->timeRef.end, rt->timeRef.start);
    ChiStats* out = &rt->stats;
    RuntimeStats* stats = rt->eventState->stats;
    const DStats* s = stats->dstats;
    const uint64_t* is = stats->istats;

    chiStatsTitle(out, "overview");

    chiStatsRow(out, "runtime");
    chiStatsTime(out, "real", delta);

#if CHI_SYSTEM_HAS_STATS
    ChiSystemStats ss;
    chiSystemStats(&ss);

    // ... continue runtime row
    chiStatsTime(out, "cpu", chiNanosAdd(ss.cpuTimeUser, ss.cpuTimeSystem));
    chiStatsTime(out, "user", ss.cpuTimeUser);
    chiStatsTime(out, "system", ss.cpuTimeSystem);
    chiStatsPercent(out, "usage", chiNanosRatio(chiNanosAdd(ss.cpuTimeUser, ss.cpuTimeSystem), delta));

    chiStatsRow(out, "memory");
    chiStatsBytes(out, "resident", ss.maxResidentSize);
    chiStatsInt(out, "pagefault", ss.pageFault);

    chiStatsRow(out, "ctx switch");
    chiStatsInt(out, "voluntary", ss.voluntaryContextSwitch);
    chiStatsInt(out, "involuntary", ss.involuntaryContextSwitch);
#endif

    chiStatsRow(out, "misc");
    chiStatsTime(out, "startup", s[DSTATS_STARTUP].sum);
    chiStatsTime(out, "shutdown", s[DSTATS_SHUTDOWN].sum);
    chiStatsInt(out, "exception", is[ISTATS_EXCEPTION]);
    chiStatsInt(out, "module", is[ISTATS_MODULE_INIT]);
}

static void statsProcessors(ChiRuntime* rt) {
    ChiStats* out = &rt->stats;
    RuntimeStats* stats = rt->eventState->stats;
    const DStats* s = stats->dstats;
    const uint64_t* is = stats->istats;
    ChiNanos delta = chiNanosDelta(rt->timeRef.end, rt->timeRef.start);

    chiStatsTitle(out, "processors");

    chiStatsRow(out, "processor");
    chiStatsPercent(out, "load", chiNanosRatio(chiNanosAdd(s[DSTATS_PROC_RUN].sum, s[DSTATS_PROC_SERVICE].sum), delta));
    chiStatsInt(out, "count", is[ISTATS_PROC_INIT]);
    chiStatsInt(out, "stall", is[ISTATS_PROC_STALL]);
    chiStatsInt(out, "requests", is[ISTATS_PROC_REQUEST]);
    chiStatsInt(out, "messages", is[ISTATS_PROC_MSG_SEND]);
    statsAdd(rt, "service", s + DSTATS_PROC_SERVICE);
    statsAdd(rt, "parked", s + DSTATS_PROC_PARK);
}

static void statsThreads(ChiRuntime* rt) {
    ChiStats* out = &rt->stats;
    RuntimeStats* stats = rt->eventState->stats;
    const DStats* s = stats->dstats;
    const uint64_t* is = stats->istats;

    chiStatsTitle(out, "threads");

    statsAdd(rt, "slice", s + DSTATS_THREAD_RUN);
    statsAdd(rt, "scheduler", s + DSTATS_THREAD_SCHED);

    chiStatsRow(out, "thread");
    chiStatsInt(out, "created", is[ISTATS_THREAD_NEW]);
    chiStatsInt(out, "switch", is[ISTATS_THREAD_SWITCH]);
    chiStatsInt(out, "terminated", is[ISTATS_THREAD_TERMINATED]);
    chiStatsInt(out, "migrated", is[ISTATS_THREAD_MIGRATE]);
    chiStatsInt(out, "takeover", is[ISTATS_THREAD_TAKEOVER]);
    chiStatsInt(out, "max", atomic_load_explicit(&stats->threadMaxCount, memory_order_relaxed));

    chiStatsRow(out, "stack");
    chiStatsInt(out, "grow", is[ISTATS_STACK_GROW]);
    chiStatsInt(out, "shrink", is[ISTATS_STACK_SHRINK]);
    chiStatsBytes(out, "init", rt->option.stack.init);
    chiStatsBytes(out, "step", rt->option.stack.step);
    chiStatsBytes(out, "max", CHI_MAX(stats->special.stackMaxSize, rt->option.stack.init));
    chiStatsBytes(out, "limit", rt->option.stack.limit);
}

typedef struct {
    size_t      id;
    const char* header;
} HistogramColumn;

static ChiStatsColumn* statsHistogramColumn(ChiStatsColumn* col, const char* header, const DStats* s, uint32_t rows, uint32_t offset, bool cumulative) {
    uint64_t* histogram = chiAllocArr(uint64_t, rows), sum = 0;
    for (uint32_t j = 0; j < rows; ++j)
        histogram[j] = (cumulative && j > 0 ? histogram[j - 1] : 0) + s->histogram[j + offset];
    for (uint32_t j = 0; j < rows; ++j)
        sum += s->histogram[j + offset];
    *col++ = (ChiStatsColumn){ .header = header, .type = CHI_STATS_INT, .ints = histogram, .sum = sum, .percent = true, .cumulative = cumulative };
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
    const HistogramDesc* h = &rt->eventState->stats->histogramDesc;
    const DStats* s = rt->eventState->stats->dstats;
    bool cumulative = rt->option.stats.cumulative;

    CHI_AUTO_ALLOC(ChiStatsColumn, column, 1 + 2 * ntcols);
    ChiStatsColumn* col = column;
    uint32_t offset = 0;
    for (; offset < h->bins && statsHistogramHideRow(s, offset, ntcols, tcol); ++offset) {}

    uint32_t rows = h->bins - offset;
    for (; rows > 0 && statsHistogramHideRow(s, offset + rows - 1, ntcols, tcol); --rows) {}

    uint64_t* durationCol = chiAllocArr(uint64_t, rows);
    for (uint32_t j = 0; j < rows; ++j)
        durationCol[j] = CHI_UN(Micros, chiNanosToMicros(binToNanos(h, j + offset)));
    *col++ = (ChiStatsColumn){ .header = "usec", .type = CHI_STATS_INT, .ints = durationCol, .sep = true };

    for (uint32_t i = 0; i < ntcols; ++i)
        col = statsHistogramColumn(col, tcol[i].header, s + tcol[i].id, rows, offset, cumulative);

    char buf[64];
    chiFmt(buf, sizeof (buf), "%s%s histogram", title, cumulative ? " cumulative" : "");
    chiStatsTable(&rt->stats, .title = buf, .rows = rows, .columns = (uint32_t)(col - column), .data = column);
}

static void statsHistogramTables(ChiRuntime* rt) {
    HistogramColumn scav[] = { { DSTATS_GC_SCAVENGER, "scav" } };
    statsHistogram(rt, "scavenger", CHI_DIM(scav), scav);

    if (rt->option.gc.mode != CHI_GC_NONE) {
        HistogramColumn ms[] = { { DSTATS_GC_MARK_SLICE,  "m slice"  },
                                 { DSTATS_GC_SWEEP_SLICE, "s slice" } };
        statsHistogram(rt, "mark & sweep", CHI_DIM(ms), ms);
    }

    HistogramColumn thread[] = { { DSTATS_THREAD_RUN, "thread" }, { DSTATS_THREAD_SCHED, "sched" } };
    statsHistogram(rt, "thread", CHI_DIM(thread), thread);
}

static void statsDestroy(ChiRuntime* rt) {
    RuntimeStats* stats = rt->eventState->stats;
    if (!stats)
        return;
    const ChiRuntimeOption* opt = &rt->option;
    statsOverview(rt);
    statsProcessors(rt);
    statsThreads(rt);
    statsHeaps(rt);
    statsGC(rt);
    if (opt->stats.histogram)
        statsHistogramTables(rt);
    chiMutexDestroy(&stats->mutex);
    chiFree(stats);
}

static RuntimeStats* statsSetup(const ChiRuntimeOption* opt) {
    if (!opt->stats.enabled)
        return 0;
    float scale = fastlog2((float)opt->stats.scale / 100.f);
    RuntimeStats* stats = (RuntimeStats*)chiZalloc(sizeof (RuntimeStats) + (DSTATS_COUNT * sizeof (uint64_t) * opt->stats.bins));
    chiMutexInit(&stats->mutex);
    stats->histogramDesc.bins = opt->stats.bins;
    stats->histogramDesc.scale = scale;
    histogramInit(stats->dstats, stats->histogramBuffer, stats->histogramDesc.bins);
    return stats;
}
