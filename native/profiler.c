#include "profiler.h"

#if CHI_PROF_ENABLED

#if CHI_CBY_SUPPORT_ENABLED
#  include "../cby/bytecode/decode.h"
#  include "../cby/cby.h"
#endif

#include <math.h>
#include <stdlib.h>
#include "error.h"
#include "event.h"
#include "hashfn.h"
#include "location.h"
#include "rand.h"
#include "runtime.h"
#include "sink.h"
#include "stack.h"
#include "strutil.h"
#include "tracepoint.h"

_Static_assert(CHI_SYSTEM_HAS_INTERRUPT, "System does not support timer interrupts.");
_Static_assert(CHI_TRACEPOINTS_ENABLED, "Tracepoints must be enabled for profiler");

typedef struct {
    ChiHash  hash;
    ChiLoc*  frame;
    uint32_t depth;
} Key;

typedef struct {
    ChiWord id;
    char* name;
} NameRecord;

typedef struct  {
    Key      key;
    uint64_t count;
} Record;

#define HT_HASH        RecordHash
#define HT_ENTRY       Record
#define HT_PREFIX      recordHash
#define HT_HASHFN(k)   k->hash
#define HT_EXISTS(e)   e->key.frame
#define HT_KEY(e)      (const Key*)&e->key
#define HT_KEYEQ(a, b) \
    CHI_UN(Hash, a->hash) == CHI_UN(Hash, b->hash)                    \
        && a->depth == b->depth                                       \
        && memeq(a->frame, b->frame, sizeof (ChiLoc) * a->depth)
#include "generic/hashtable.h"

#define HT_HASH        NameHash
#define HT_ENTRY       NameRecord
#define HT_KEY(e)      e->id
#define HT_HASHFN(k)   chiHashWord(k)
#define HT_EXISTS(e)   e->name
#define HT_PREFIX      nameHash
#include "generic/hashtable.h"

#define VEC_TYPE void*
#define VEC_PREFIX ptrVec
#define VEC PtrVec
#include "generic/vec.h"

typedef struct ProfilerWSLink_ ProfilerWSLink;
struct ProfilerWSLink_ { ProfilerWSLink *prev, *next; };
struct ChiProfilerWS_ {
    char*       workerName;
    RecordHash  recordHash;
    NameHash    nameHash;
    Key         key;
    ProfilerWSLink link;
    ChiWorker*  worker;
    ChiLoc      frame[0];
};

#define LIST        ProfilerWSList
#define LIST_LINK   ProfilerWSLink
#define LIST_PREFIX profilerWSList
#define LIST_ELEM   ChiProfilerWS
#include "generic/list.h"

struct ChiProfiler_ {
    ChiProfOption profOpt;
    RecordHash    recordHash;
    PtrVec        freeVec;
    ChiTimer      timer;
    ChiMutex      timerMutex;
    ChiMutex      wsMutex;
    ProfilerWSList wsList;
    ChiRandState  randState;
};

CHI_INL void addFrame(Key* k, ChiLocType type, const void* id) {
    k->frame[k->depth].type = type;
    k->frame[k->depth].id = id;
    ++k->depth;
}

static ChiMicros profilerTimerHandler(ChiTimer* timer) {
    ChiProfiler* prof = CHI_OUTER(ChiProfiler, timer, timer);
    {
        CHI_LOCK_MUTEX(&prof->wsMutex);
        CHI_LIST_FOREACH(ChiProfilerWS, link, ws, &prof->wsList) {
            chiTraceTriggerSet(ws->worker,
                               prof->profOpt.alloc ? -1 :
                               chiTraceTriggerGet(ws->worker) + 1);
        }
    }
    uint64_t
        interval = 1000000 / prof->profOpt.rate,
        jitter = chiRand(prof->randState) % (interval / 2) - (interval / 4);
    return chiMicros(interval + jitter % (interval / 2) - (interval / 4));
}

static void printFrame(ChiSink* sink, const ChiLoc* frame, bool separateLoc, bool mangled) {
    switch (frame->type) {
    case CHI_LOC_NAME:
        CHI_BUG("CHI_LOC_NAME cannot be handled here");
    case CHI_LOC_THREAD:
        chiSinkFmt(sink, "[thread %s]", (const char*)frame->id);
        break;
    case CHI_LOC_INCOMPLETE:
        chiSinkPutc(sink, '-');
        break;
    case CHI_LOC_WORKER:
        chiSinkFmt(sink, "[%s]", (const char*)frame->id);
        break;
    case CHI_LOC_FFI:
#if CHI_CBY_SUPPORT_ENABLED
        {
            const CbyCode* IP = (const CbyCode*)frame->id + 4;
            chiSinkFmt(sink, "[ffi %S]", FETCH_STRING);
            break;
        }
#else
        CHI_BUG("CHI_LOC_FFI is only allowed if the interpreter is enabled");
#endif
    case CHI_LOC_NATIVE:
    case CHI_LOC_INTERP:
        {
            ChiLocResolve resolve;
            chiLocResolve(&resolve, *frame, mangled);
            chiLocFmt(sink, &resolve.loc, CHI_LOCFMT_FN | CHI_LOCFMT_INTERP |
                      (separateLoc ? CHI_LOCFMT_FILESEP : CHI_LOCFMT_FILE));
            break;
        }
    }
}

static void printTrace(ChiSink* sink, const Key* k, bool mangled) {
    for (uint32_t i = k->depth; i --> 0;) {
        if (k->frame[i].type == CHI_LOC_NAME) {
            const char* name = (const char*)k->frame[i].id, *tok, *end;
            while ((tok = strsplit(&name, ';', &end))) {
                chiSinkFmt(sink, "[%b]", (uint32_t)(end - tok), tok);
                if (name)
                    chiSinkPutc(sink, ';');
            }
        } else {
            printFrame(sink, k->frame + i, false, mangled);
        }
        if (i)
            chiSinkPutc(sink, ';');
    }
}

static void printSamples(const char* file, ChiSinkColor color, bool mangled, const RecordHash* rh) {
    CHI_AUTO_SINK(sink, chiSinkFileTryNew(file, CHI_KiB(8), false, color));
    if (!sink)
        return;

    CHI_HT_FOREACH(RecordHash, r, rh) {
        printTrace(sink, &r->key, mangled);
        chiSinkFmt(sink, " %ju\n", r->count);
    }

    chiSinkFmt(chiStderr,
               "Profiler: Stack samples written to '%s'.\n"
               "          Use 'chili-flame' or 'chili-callgrind' to take a look!\n", file);
}

#if CHI_STATS_ENABLED
typedef struct  {
    char*    name;
    uint64_t self, sum;
} AccumRecord;

#define HT_HASH        AccumHash
#define HT_ENTRY       AccumRecord
#define HT_PREFIX      accumHash
#define HT_KEY(e)      chiStringRef(e->name)
#define HT_EXISTS(e)   e->name
#define HT_KEYEQ(a, b) chiStringRefEq(a, b)
#define HT_HASHFN      chiHashStringRef
#include "generic/hashtable.h"

#define S_SUFFIX     AccumRecordBySelf
#define S_ELEM       AccumRecord
#define S_LESS(a, b) ((b)->self == (a)->self ? (b)->sum < (a)->sum : (b)->self < (a)->self)
#include "generic/sort.h"

#define S_SUFFIX     AccumRecordBySum
#define S_ELEM       AccumRecord
#define S_LESS(a, b) ((b)->sum == (a)->sum ? (b)->self < (a)->self : (b)->sum < (a)->sum)
#include "generic/sort.h"

static void addAccumRecord(AccumHash* accumHash, ChiStringRef name, uint64_t count, bool self) {
    AccumRecord* a;
    if (accumHashCreate(accumHash, name, &a))
        a->name = chiCStringUnsafeDup(name);
    a->self += self * count;
    a->sum += count;
}

static void accumProfile(AccumHash* accumHash, const RecordHash* recordHash,
                         uint64_t* totalSum, uint64_t* totalSelf, bool mangled) {
    CHI_STRING_SINK(sink);
    CHI_HT_FOREACH(RecordHash, r, recordHash) {
        // ignore suspend stacks
        if (r->key.depth >= 1 && r->key.frame[0].type == CHI_LOC_NAME &&
            (streq((const char*)r->key.frame[0].id, "runtime;suspend") ||
             streq((const char*)r->key.frame[0].id, "runtime;sync")))
            continue;

        *totalSelf += r->count;
        *totalSum += r->key.depth * r->count;
        for (uint32_t i = r->key.depth; i --> 0;) {
            if (r->key.frame[i].type == CHI_LOC_NAME) {
                const char* name = (const char*)r->key.frame[i].id, *tok, *end;
                while ((tok = strsplit(&name, ';', &end))) {
                    chiSinkFmt(sink, "[%b]", (uint32_t)(end - tok), tok);
                    addAccumRecord(accumHash, chiSinkString(sink), r->count, !i && !name);
                }
            } else {
                printFrame(sink, r->key.frame + i, true, mangled);
                addAccumRecord(accumHash, chiSinkString(sink), r->count, !i);
            }
        }
    }
}

static void addFlatTable(ChiStats* stats, uint32_t rows, const char* title, uint64_t totalSum, uint64_t totalSelf, const AccumHash* accum) {
    rows = (uint32_t)CHI_MIN(accum->used, rows);
    uint64_t* selfCol = chiAllocArr(uint64_t, rows);
    uint64_t* sumCol = chiAllocArr(uint64_t, rows);
    ChiStringRef* nameCol = chiAllocArr(ChiStringRef, rows);
    ChiStringRef* locCol = chiAllocArr(ChiStringRef, rows);

    uint32_t i = 0;
    CHI_HT_FOREACH(AccumHash, a, accum) {
        if (i >= rows)
            break;
        ChiStringRef n = chiStringRef(a->name);
        const uint8_t* p = (const uint8_t*)memchr(n.bytes, '\n', n.size);
        if (p) {
            uint32_t s = (uint32_t)(p - n.bytes);
            locCol[i].bytes = p + 1;
            locCol[i].size = n.size - s - 1;
            nameCol[i].bytes = n.bytes;
            nameCol[i].size = s;
        } else {
            nameCol[i] = n;
            locCol[i] = CHI_EMPTY_STRINGREF;
        }
        selfCol[i] = a->self;
        sumCol[i] = a->sum;
        ++i;
    }

    const ChiStatsColumn column[] = {
        { .header = "%",        .type = CHI_STATS_INT,    .ints    = selfCol, .sum = totalSelf, .percent = true },
        { .header = "self",     .type = CHI_STATS_INT,    .ints    = selfCol, .sep = true },
        { .header = "%",        .type = CHI_STATS_INT,    .ints    = sumCol,  .sum = totalSum, .percent = true },
        { .header = "sum",      .type = CHI_STATS_INT,    .ints    = sumCol,  .sep = true },
        { .header = "function", .type = CHI_STATS_PATH, .strings = nameCol, .left = true, .sep = true },
        { .header = "location", .type = CHI_STATS_PATH, .strings = locCol,  .left = true },
    };
    chiStatsTable(stats, .title = title, .rows = rows, .columns = CHI_DIM(column), .data = column);
}
#endif

CHI_INL void recordAdd(RecordHash* hash, Key* k, uint64_t count, uint32_t maxStacks) {
    Record* r;
    k->hash = chiHashBytes(k->frame, sizeof (ChiLoc) * k->depth);
    if (recordHashCreate(hash, k, &r)) {
        if (hash->used <= maxStacks) {
            r->key.frame = chiAllocArr(ChiLoc, k->depth);
            memcpy(r->key.frame, k->frame, k->depth * sizeof (ChiLoc));
            r->key.depth = k->depth;
            r->key.hash = k->hash;
            r->count = count;
        } else {
            --hash->used;
        }
    } else {
        r->count += count;
    }
}

static void storeRecord(ChiWorker* worker, Key* key, size_t alloc, uintptr_t stack) {
    ChiRuntime* rt = worker->rt;
    CHI_ASSERT(key->depth <= rt->profiler->profOpt.maxDepth);

    uint64_t count = rt->profiler->profOpt.alloc ? alloc : (uint64_t)chiTraceTriggerGet(worker);
    chiTraceTriggerSet(worker, 0);
    recordAdd(&worker->profilerWS->recordHash, key, count, rt->profiler->profOpt.maxStacks);

    if (chiEventEnabled(worker, PROF_TRACE)) {
        CHI_STRING_SINK(sink);
        printTrace(sink, key, rt->option.stack.traceMangled);
        chiEvent(worker, PROF_TRACE, .trace = chiSinkString(sink), .stack = stack);
    }

    key->depth = 0;
}

static void addThreadFrame(ChiProcessor* proc, Key* key) {
    Chili name = chiThreadName(proc->thread);
    if (!chiTrue(name)) {
        addFrame(key, CHI_LOC_NAME, "unnamed thread");
    } else {
        NameRecord* nr;
        uint32_t tid = chiThreadId(proc->thread);
        if (nameHashCreate(&proc->worker->profilerWS->nameHash, tid, &nr)) {
            nr->name = chiCStringUnsafeDup(chiStringRef(&name));
            nr->id = tid;
        }
        addFrame(key, CHI_LOC_THREAD, nr->name);
    }
}

static void captureStack(ChiRuntime* rt, ChiStack* stack, Chili* sp, Key* key) {
    ChiStackWalk w = chiStackWalkInit(stack, sp,
                                      // three extra frames: incomplete, thread, processor
                                      rt->profiler->profOpt.maxDepth - 3,
                                      rt->option.stack.traceCycles);
    while (chiStackWalk(&w)) {
#if CHI_CBY_SUPPORT_ENABLED
        if (chiFrame(w.frame) == CHI_FRAME_INTERP) {
            CbyFn* fn = chiToCbyFn(w.frame[-3]);
            addFrame(key, CHI_LOC_INTERP, chiToIP(fn->ip));
            continue;
        }
#endif
        addFrame(key, CHI_LOC_NATIVE, chiToCont(*w.frame));
    }
    if (w.frame >= stack->base)
        addFrame(key, CHI_LOC_INCOMPLETE, 0);
}

void chiTraceHandler(const ChiTracePoint* tp) {
    ChiWorker* worker = tp->worker;
    Key* key = &worker->profilerWS->key;
    if (tp->name)
        addFrame(key, CHI_LOC_NAME, tp->name);
    uintptr_t stack = 0;
    if (tp->cont) {
        addFrame(key, CHI_LOC_NATIVE, tp->cont);
        Chili stackObj = chiThreadStack(tp->proc->thread);
        stack = chiAddress(stackObj);
        ChiStack* s = chiToStack(stackObj);
        captureStack(worker->rt, s, tp->frame ? tp->frame : s->sp, key);
    }
    if (tp->thread)
        addThreadFrame(tp->proc, key);
    addFrame(key, CHI_LOC_WORKER, worker->profilerWS->workerName);
    storeRecord(worker, key, tp->alloc, stack);
}

#if CHI_CBY_SUPPORT_ENABLED
void _profTrace(ChiProcessor*, Chili, Chili*, const CbyCode*, size_t);
void _profTrace(ChiProcessor* proc, Chili fn, Chili* frame, const CbyCode* ffi, size_t alloc) {
    CbyFn* f = chiToCbyFn(fn);
    ChiWorker* worker = proc->worker;
    Key* key = &worker->profilerWS->key;
    if (ffi)
        addFrame(key, CHI_LOC_FFI, ffi);
    addFrame(key, CHI_LOC_INTERP, chiToIP(f->ip));
    Chili stack = chiThreadStack(proc->thread);
    captureStack(worker->rt, chiToStack(stack), frame, key);
    addThreadFrame(proc, key);
    addFrame(key, CHI_LOC_WORKER, worker->profilerWS->workerName);
    storeRecord(worker, key, alloc, chiAddress(stack));
}
#endif

static void profilerEnable(ChiWorker* worker, bool enabled) {
    ChiProfiler* prof = worker->rt->profiler;
    CHI_LOCK_MUTEX(&prof->timerMutex);
    if (enabled == (prof->timer.handler != 0))
        return;
    if (enabled) {
        prof->timer = (ChiTimer)
            { .timeout = chiMicros(1000000 / prof->profOpt.rate),
              .handler = profilerTimerHandler };
        chiTimerInstall(&prof->timer);
        chiEvent0(worker, PROF_ENABLED);
    } else {
        chiTimerRemove(&prof->timer);
        chiEvent0(worker, PROF_DISABLED);
    }
}

static void profilerDestroy(ChiWorker* worker) {
    ChiRuntime* rt = worker->rt;
    ChiProfiler* prof = rt->profiler;
    const ChiRuntimeOption* opt = &rt->option;

    profilerEnable(worker, false);

    if (!CHI_TRACEPOINTS_CONT_ENABLED)
        chiSinkPuts(chiStderr,
                    "Profiler: Continuation tracepoints are disabled.\n"
                    "          Profile might contain unknown native functions.\n"
                    "          Use the build with full profiling support instead!\n");

#if CHI_STATS_ENABLED
    if (prof->profOpt.flat) {
        CHI_HT_AUTO(AccumHash, accum);
        uint64_t totalSum = 0, totalSelf = 0;
        accumProfile(&accum, &prof->recordHash, &totalSum, &totalSelf, opt->stack.traceMangled);
        sortAccumRecordBySelf(accum.entry, accum.capacity);
        addFlatTable(&rt->stats, opt->stats.tableRows,
                     prof->profOpt.alloc ? "allocation profile self" : "profile self", totalSum, totalSelf, &accum);
        sortAccumRecordBySum(accum.entry, accum.capacity);
        addFlatTable(&rt->stats, opt->stats.tableRows,
                     prof->profOpt.alloc ? "allocation profile sum" : "profile sum", totalSum, totalSelf, &accum);
        CHI_HT_FOREACH(AccumHash, r, &accum)
            chiFree(r->name);
    }
#endif

    if (prof->profOpt.file[0])
        printSamples(prof->profOpt.file, CHI_AND(CHI_COLOR_ENABLED, opt->color),
                     opt->stack.traceMangled, &prof->recordHash);

    CHI_HT_FOREACH(RecordHash, r, &prof->recordHash)
        chiFree(r->key.frame);
    recordHashFree(&prof->recordHash);
    for (size_t i = 0; i < prof->freeVec.used; ++i)
        chiFree(CHI_VEC_AT(&prof->freeVec, i));
    ptrVecFree(&prof->freeVec);
    chiMutexDestroy(&prof->wsMutex);
    chiMutexDestroy(&prof->timerMutex);
    chiFree(prof);
}

static void profilerWorkerStop(ChiHookType CHI_UNUSED(type), void* ctx) {
    ChiWorker* worker = (ChiWorker*)ctx;
    ChiProfiler* prof = worker->rt->profiler;
    ChiProfilerWS* ws = worker->profilerWS;
    uint32_t maxStacks = prof->profOpt.maxStacks;
    CHI_ASSERT(worker);
    CHI_ASSERT(ws);
    {
        CHI_LOCK_MUTEX(&prof->wsMutex);
        CHI_HT_FOREACH(RecordHash, r, &ws->recordHash) {
            recordAdd(&prof->recordHash, &r->key, r->count, maxStacks);
            chiFree(r->key.frame);
        }
        recordHashFree(&ws->recordHash);
        CHI_HT_FOREACH(NameHash, r, &ws->nameHash)
            ptrVecAppend(&prof->freeVec, r->name);
        ptrVecAppend(&prof->freeVec, ws->workerName);
    }
    nameHashFree(&ws->nameHash);
    profilerWSListDelete(ws);
    chiFree(ws);
    if (chiWorkerMain(worker))
        profilerDestroy(worker);
}

static void profilerWorkerStart(ChiHookType CHI_UNUSED(type), void* ctx) {
    ChiWorker* worker = (ChiWorker*)ctx;
    ChiProfiler* prof = worker->rt->profiler;
    size_t framesSize = sizeof (ChiLoc) * prof->profOpt.maxDepth;
    ChiProfilerWS* ws = worker->profilerWS = (ChiProfilerWS*)chiZalloc(sizeof (ChiProfilerWS) + framesSize);
    profilerWSListPoison(ws);
    ws->worker = worker;
    ws->workerName = chiCStringDup(worker->name);
    ws->key.frame = ws->frame;
    {
        CHI_LOCK_MUTEX(&prof->wsMutex);
        profilerWSListAppend(&prof->wsList, ws);
    }
    if (chiWorkerMain(worker))
        profilerEnable(worker, !prof->profOpt.off);
}

void chiProfilerEnable(bool enabled) {
    profilerEnable(CHI_CURRENT_PROCESSOR->worker, enabled);
}

void chiProfilerSetup(ChiRuntime* rt, const ChiProfOption* profOpt) {
    ChiProfiler* prof = chiZallocObj(ChiProfiler);
    chiRandInit(prof->randState, 0);
    prof->profOpt = *profOpt;
    chiMutexInit(&prof->wsMutex);
    chiMutexInit(&prof->timerMutex);
    rt->profiler = prof;
    profilerWSListInit(&prof->wsList);
    chiHook(rt, CHI_HOOK_WORKER_START, profilerWorkerStart);
    chiHook(rt, CHI_HOOK_WORKER_STOP, profilerWorkerStop);
}

static ChiOptionResult setProfFile(const ChiOptionParser* CHI_UNUSED(parser),
                                   const ChiOptionList* list,
                                   const ChiOption* CHI_UNUSED(opt),
                                   const void* CHI_UNUSED(val)) {
    ChiProfOption* opt = (ChiProfOption*)list->target;
    chiFmt(opt->file, sizeof (opt->file), "prof.%u", chiPid());
    return CHI_OPTRESULT_OK;
}

#define CHI_OPT_STRUCT ChiProfOption
CHI_INTERN const ChiOption chiProfOptionList[] = {
    CHI_OPT_TITLE("PROFILER")
    CHI_OPT_FLAG(flat, "flat", "Print flat profile")
    CHI_OPT_CB(FLAG, setProfFile, "prof", "Write stack samples to 'prof.pid'")
    CHI_OPT_STRING(file, "prof:o", "Write stack samples log to given file")
    CHI_OPT_UINT32(rate, 1, 10000, "prof:r", "Sample rate, per second")
    CHI_OPT_UINT32(maxDepth, 5, 10000, "prof:d", "Stack capturing depth")
    CHI_OPT_UINT32(maxStacks, 1, 10000, "prof:s", "Number of different stacks to record")
    CHI_OPT_FLAG(alloc, "prof:a", "Profile allocations")
    CHI_OPT_FLAG(off, "prof:off", "Start with sampling turned off")
    CHI_OPT_END
};
#undef CHI_OPT_STRUCT

#endif
