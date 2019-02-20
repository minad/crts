#include "private.h"

#ifdef CHI_PROF_CBY_SUPPORT
#  define CHI_PROF_API
#else
#  define CHI_PROF_API CHI_WEAK
#endif

#if CHI_PROF_ENABLED

#include "profiler.h"
#include "runtime.h"
#include "sink.h"
#include "event.h"
#include "stack.h"
#include "strutil.h"
#include "thread.h"
#include "trace.h"
#include "mem.h"
#include "hashfn.h"
#include "location.h"
#include "rand.h"
#include "error.h"
#include <math.h>
#include <stdlib.h>

_Static_assert(CHI_SYSTEM_HAS_INTERRUPT, "System does not support timer interrupts.");

typedef struct {
    ChiHashIndex hash;
    ChiLoc*      frame;
    uint32_t     depth;
} Key;

typedef struct  {
    char*    name;
    uint64_t self, sum;
} AccumRecord;

typedef struct {
    ChiWord id;
    char* name;
} NameRecord;

typedef struct  {
    Key      key;
    uint64_t count;
} Record;

#define HASH        RecordHash
#define ENTRY       Record
#define PREFIX      recordHash
#define HASHFN(k)   k->hash
#define EXISTS(e)   e->key.frame
#define KEY(e)      (const Key*)&e->key
#define KEYEQ(a, b) \
    CHI_UN(HashIndex, a->hash) == CHI_UN(HashIndex, b->hash)          \
        && a->depth == b->depth                                       \
        && memeq(a->frame, b->frame, sizeof (ChiLoc) * a->depth)
#include "hashtable.h"

#define HASH        AccumHash
#define ENTRY       AccumRecord
#define PREFIX      accumHash
#define KEY(e)      chiStringRef(e->name)
#define EXISTS(e)   e->name
#define KEYEQ(a, b) chiStringRefEq(a, b)
#define HASHFN      chiHashStringRef
#include "hashtable.h"

#define HASH        NameHash
#define ENTRY       NameRecord
#define KEY(e)      e->id
#define HASHFN(k)   chiHashWord(k)
#define EXISTS(e)   e->name
#define PREFIX      nameHash
#include "hashtable.h"

#define S_SUFFIX     AccumRecordBySelf
#define S_ELEM       AccumRecord
#define S_LESS(a, b) ((b)->self == (a)->self ? (b)->sum < (a)->sum : (b)->self < (a)->self)
#include "sort.h"

#define S_SUFFIX     AccumRecordBySum
#define S_ELEM       AccumRecord
#define S_LESS(a, b) ((b)->sum == (a)->sum ? (b)->self < (a)->self : (b)->sum < (a)->sum)
#include "sort.h"

#define VEC_TYPE void*
#define VEC_PREFIX ptrVec
#define VEC PtrVec
#include "vector.h"

struct ChiProfilerWS_ {
    char*       workerName;
    RecordHash  recordHash;
    NameHash    nameHash;
    Key         key;
    ChiList     list;
    ChiWorker*  worker;
    ChiLoc      frame[0];
};

struct ChiProfiler_ {
    ChiProfOption profOpt;
    RecordHash    recordHash;
    PtrVec        freeVec;
    ChiTimer      timer;
    ChiMutex      mutex;
    ChiList       wsList;
    ChiRandState  randState;
};

CHI_INL void addFrame(Key* k, uintptr_t type, const void* id) {
    CHI_ASSERT(k->depth < CHI_CURRENT_RUNTIME->profiler->profOpt.maxDepth);
    k->frame[k->depth].type = type;
    k->frame[k->depth].id = id;
    ++k->depth;
}

static ChiMicros profilerTimerHandler(ChiTimer* timer) {
    ChiProfiler* prof = CHI_OUTER(ChiProfiler, timer, timer);
    {
        CHI_LOCK(&prof->mutex);
        CHI_LIST_FOREACH(ChiProfilerWS, list, ws, &prof->wsList) {
            CHI_ASSERT(ws->worker->tp);
            if (prof->profOpt.alloc)
                *ws->worker->tp = -1;
            else
                ++(*ws->worker->tp);
        }
    }
    uint64_t
        interval = 1000000 / prof->profOpt.rate,
        jitter = chiRand(prof->randState) % (interval / 2) - (interval / 4);
    return (ChiMicros){interval + jitter % (interval / 2) - (interval / 4)};
}

static void printFrame(ChiSink* sink, const ChiLoc* frame, bool separateLoc) {
    switch (frame->type) {
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
#ifdef CHI_PROF_CBY_SUPPORT
        {
            const CbyCode* IP = (const CbyCode*)frame->id + 4;
            chiSinkFmt(sink, "[ffi %S]", FETCH_STRING);
            break;
        }
#else
        CHI_BUG("CHI_LOC_FFI is only allowed if the interpreter is enabled");
#endif
    default:
        {
            ChiLocLookup lookup;
            chiLocLookup(&lookup, *frame);
            chiLocFmt(sink, &lookup.loc, CHI_LOCFMT_MODFN | CHI_LOCFMT_INTERP |
                        (separateLoc ? CHI_LOCFMT_FILESEP : CHI_LOCFMT_FILE));
            break;
        }
    }
}

static void printTrace(ChiSink* sink, const Key* k) {
    for (uint32_t i = k->depth; i --> 0;) {
        if (k->frame[i].type == CHI_LOC_NAME) {
            const char* name = (const char*)k->frame[i].id, *tok, *end;
            while ((tok = strsplit(&name, ';', &end))) {
                chiSinkFmt(sink, "[%b]", (uint32_t)(end - tok), tok);
                if (name)
                    chiSinkPutc(sink, ';');
            }
        } else {
            printFrame(sink, k->frame + i, false);
        }
        if (i)
            chiSinkPutc(sink, ';');
    }
}

static void printSamples(const char* file, ChiSinkColor color, const RecordHash* rh) {
    CHI_AUTO_SINK(sink, chiSinkFileTryNew(file, color));
    if (!sink)
        return;

    HASH_FOREACH(recordHash, r, rh) {
        printTrace(sink, &r->key);
        chiSinkFmt(sink, " %ju\n", r->count);
    }

    chiSinkFmt(chiStderr,
               "Profiler: Stack samples written to '%s'.\n"
               "          Use 'chili-flame' or 'chili-callgrind' to take a look!\n", file);
}

static void addAccumRecord(AccumHash* accumHash, ChiStringRef name, uint64_t count, bool self) {
    AccumRecord* a;
    if (accumHashCreate(accumHash, name, &a))
        a->name = chiCStringUnsafeDup(name);
    a->self += self * count;
    a->sum += count;
}

static void accumProfile(AccumHash* accumHash, const RecordHash* recordHash, uint64_t* totalSum, uint64_t* totalSelf) {
    CHI_STRING_SINK(sink);
    HASH_FOREACH(recordHash, r, recordHash) {
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
                printFrame(sink, r->key.frame + i, true);
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
    HASH_FOREACH(accumHash, a, accum) {
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
        { .header = "function", .type = CHI_STATS_STRING, .strings = nameCol, .left = true, .sep = true },
        { .header = "location", .type = CHI_STATS_STRING, .strings = locCol,  .left = true },
    };
    chiStatsTable(stats, .title = title, .rows = rows, .columns = CHI_DIM(column), .data = column);
}

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

static void storeRecord(ChiWorker* worker, Key* key, size_t alloc) {
    uint64_t count = worker->rt->profiler->profOpt.alloc ? alloc : (uint64_t)*worker->tp;
    *worker->tp = 0;
    recordAdd(&worker->profilerWS->recordHash, key, count, worker->rt->profiler->profOpt.maxStacks);

    if (CHI_EVENT_P(worker, PROF_TRACE)) {
        CHI_STRING_SINK(sink);
        printTrace(sink, key);
        CHI_EVENT(worker, PROF_TRACE, .trace = chiSinkString(sink));
    }

    key->depth = 0;
}

static void addThreadFrame(ChiProcessor* proc, Key* key) {
    Chili name = chiThreadName(proc->thread);
    if (!chiTrue(name)) {
        addFrame(key, CHI_LOC_NAME, "unnamed thread");
    } else {
        NameRecord* nr;
        ChiWord id = chiToUnboxed(chiToThread(proc->thread)->tid);
        if (nameHashCreate(&proc->worker.profilerWS->nameHash, id, &nr)) {
            nr->name = chiCStringUnsafeDup(chiStringRef(&name));
            nr->id = id;
        }
        addFrame(key, CHI_LOC_THREAD, nr->name);
    }
}

static void captureStack(ChiWorker* worker, const ChiStack* stack, Key* key) {
    uint32_t maxDepth = worker->rt->profiler->profOpt.maxDepth;
    for (const Chili* p = stack->sp - 1, *last = 0; p >= stack->base; p -= chiFrameSize(p)) {
        if (!chiFrameIdentical(last, p)) {
            last = p;

#ifdef CHI_PROF_CBY_SUPPORT
            if (chiFrame(p) == CHI_FRAME_INTERP) {
                CbyFn* fn = chiToCbyFn(p[-3]);
                addFrame(key, (uintptr_t)cbyInterpModuleCode(fn->module), chiToIP(fn->ip));
                continue;
            }
#endif
            addFrame(key, CHI_LOC_NATIVE, chiToCont(*p));
        }

        if (key->depth + 3 == maxDepth) {
            addFrame(key, CHI_LOC_INCOMPLETE, 0);
            break;
        }
    }
}

CHI_PROF_API void chiTraceHandler(ChiWorker* worker, const ChiTracePoint* tp) {
    Key* key = &worker->profilerWS->key;
    if (tp->name)
        addFrame(key, CHI_LOC_NAME, tp->name);
    if (tp->cont) {
        addFrame(key, CHI_LOC_NATIVE, tp->cont);
        ChiStack* stack = chiToStack(chiThreadStack(chiWorkerToProcessor(worker)->thread));
        if (tp->frame)
            stack->sp = tp->frame;
        captureStack(worker, stack, key);
    }
    if (tp->thread)
        addThreadFrame(chiWorkerToProcessor(worker), key);
    addFrame(key, CHI_LOC_WORKER, worker->profilerWS->workerName);
    storeRecord(worker, key, tp->alloc);
}

#ifdef CHI_PROF_CBY_SUPPORT
static CHI_NOINL void interpTrace(ChiProcessor* proc, Chili fn, Chili* frame, const CbyCode* ffi, size_t alloc) {
    CbyFn* f = chiToCbyFn(fn);
    ChiWorker* worker = &proc->worker;
    Key* key = &worker->profilerWS->key;
    if (ffi)
        addFrame(key, CHI_LOC_FFI, ffi);
    addFrame(key, (uintptr_t)cbyInterpModuleCode(f->module), chiToIP(f->ip));
    ChiStack* stack = chiToStack(chiThreadStack(proc->thread));
    stack->sp = frame;
    captureStack(worker, stack, key);
    addThreadFrame(proc, key);
    addFrame(key, CHI_LOC_WORKER, worker->profilerWS->workerName);
    storeRecord(worker, key, alloc);
}
#endif

static void profilerEnable(ChiWorker* worker, bool enabled) {
    {
        ChiProfiler* prof = worker->rt->profiler;
        CHI_LOCK(&prof->mutex);
        if (enabled == (prof->timer.handler != 0))
            return;
        if (enabled) {
            prof->timer = (ChiTimer){
                                     .timeout = (ChiMicros){ 1000000 / prof->profOpt.rate },
                                     .handler = profilerTimerHandler,
            };
            chiTimerInstall(&prof->timer);
        } else {
            chiTimerRemove(&prof->timer);
            prof->timer.handler = 0;
        }
    }
    if (enabled)
        CHI_EVENT0(worker, PROF_ENABLED);
    else
        CHI_EVENT0(worker, PROF_DISABLED);
}

static void profilerDestroy(ChiWorker* worker, ChiHookType CHI_UNUSED(type)) {
    ChiRuntime* rt = worker->rt;
    ChiProfiler* prof = rt->profiler;

    profilerEnable(worker, false);

    if (prof->profOpt.flat) {
        HASH_AUTO(AccumHash, accum);
        uint64_t totalSum = 0, totalSelf = 0;
        accumProfile(&accum, &prof->recordHash, &totalSum, &totalSelf);
        sortAccumRecordBySelf(accum.entry, accum.capacity);
        addFlatTable(&rt->stats, rt->option.stats->tableRows, prof->profOpt.alloc ? "allocation profile self" : "profile self", totalSum, totalSelf, &accum);
        sortAccumRecordBySum(accum.entry, accum.capacity);
        addFlatTable(&rt->stats, rt->option.stats->tableRows, prof->profOpt.alloc ? "allocation profile sum" : "profile sum", totalSum, totalSelf, &accum);
        HASH_FOREACH(accumHash, r, &accum)
            chiFree(r->name);
    }

    if (!CHI_TRACEPOINTS_ENABLED)
        chiSinkPuts(chiStderr,
                    "Profiler: Profile might contain unknown native functions, since tracepoints are disabled.\n"
                    "          Use the runtime with profiling support instead!\n");

    if (prof->profOpt.file[0])
        printSamples(prof->profOpt.file, rt->option.color, &prof->recordHash);

    HASH_FOREACH(recordHash, r, &prof->recordHash)
        chiFree(r->key.frame);
    recordHashDestroy(&prof->recordHash);
    for (size_t i = 0; i < prof->freeVec.used; ++i)
        chiFree(CHI_VEC_AT(&prof->freeVec, i));
    ptrVecFree(&prof->freeVec);
    chiMutexDestroy(&prof->mutex);
    chiFree(prof);
}

static void profilerWorkerStop(ChiWorker* worker, ChiHookType CHI_UNUSED(type)) {
    ChiProfiler* prof = worker->rt->profiler;
    ChiProfilerWS* ws = worker->profilerWS;
    uint32_t maxStacks = prof->profOpt.maxStacks;
    CHI_ASSERT(worker);
    CHI_ASSERT(ws);
    CHI_LOCK(&prof->mutex);
    HASH_FOREACH(recordHash, r, &ws->recordHash) {
        recordAdd(&prof->recordHash, &r->key, r->count, maxStacks);
        chiFree(r->key.frame);
    }
    recordHashDestroy(&ws->recordHash);
    HASH_FOREACH(nameHash, r, &ws->nameHash)
        ptrVecPush(&prof->freeVec, r->name);
    ptrVecPush(&prof->freeVec, ws->workerName);
    nameHashDestroy(&ws->nameHash);
    chiListDelete(&ws->list);
    chiFree(ws);
}

static void profilerWorkerStart(ChiWorker* worker, ChiHookType CHI_UNUSED(type)) {
    ChiProfiler* prof = worker->rt->profiler;
    size_t framesSize = sizeof (ChiLoc) * prof->profOpt.maxDepth;
    ChiProfilerWS* ws = worker->profilerWS = (ChiProfilerWS*)chiZalloc(sizeof (ChiProfilerWS) + framesSize);
    chiListPoison(&ws->list);
    ws->worker = worker;
    ws->workerName = chiCStringDup(worker->name);
    ws->key.frame = ws->frame;
    {
        CHI_LOCK(&prof->mutex);
        chiListAppend(&prof->wsList, &ws->list);
    }
    if (chiMainWorker(worker))
        profilerEnable(worker, !prof->profOpt.off);
}

CHI_PROF_API void chiProfilerEnable(bool enabled) {
    profilerEnable(CHI_CURRENT_WORKER, enabled);
}

CHI_PROF_API void chiProfilerSetup(ChiRuntime* rt, const ChiProfOption* profOpt) {
    ChiProfiler* prof = chiZallocObj(ChiProfiler);
    chiRandInit(prof->randState, 0);
    prof->profOpt = *profOpt;
    chiMutexInit(&prof->mutex);
    rt->profiler = prof;
    chiListInit(&prof->wsList);
    chiHook(rt, CHI_HOOK_RUNTIME_DESTROY, profilerDestroy);
    chiHook(rt, CHI_HOOK_WORKER_START, profilerWorkerStart);
    chiHook(rt, CHI_HOOK_WORKER_STOP, profilerWorkerStop);
}

static ChiOptionResult setProfFile(const ChiOptionParser* CHI_UNUSED(parser),
                                   const ChiOptionList* list,
                                   const ChiOption* CHI_UNUSED(opt),
                                   const void* CHI_UNUSED(val)) {
    ChiProfOption* opt = (ChiProfOption*)list->target;
    chiFmt(opt->file, sizeof (opt->file), "prof.%u%s", chiPid(), chiSinkCompress());
    return CHI_OPTRESULT_OK;
}

#define CHI_OPT_TARGET ChiProfOption
const ChiOption chiProfOptionList[] = {
    CHI_OPT_DESC_TITLE("PROFILER")
    CHI_OPT_DESC_FLAG(flat, "flat", "Print flat profile")
    CHI_OPT_DESC_CB(FLAG, setProfFile, "prof", "Write stack samples to 'prof.pid'")
    CHI_OPT_DESC_STRING(file, "prof:o", "Write stack samples log to given file")
    CHI_OPT_DESC_UINT32(rate, 1, 10000, "prof:r", "Sample rate, per second")
    CHI_OPT_DESC_UINT32(maxDepth, 5, 10000, "prof:d", "Stack capturing depth")
    CHI_OPT_DESC_UINT32(maxStacks, 1, 10000, "prof:s", "Number of different stacks to record")
    CHI_OPT_DESC_FLAG(alloc, "prof:a", "Profile allocations")
    CHI_OPT_DESC_FLAG(off, "prof:off", "Start with sampling turned off")
    CHI_OPT_DESC_END
};
#undef CHI_OPT_TARGET

#else
CHI_PROF_API void chiProfilerEnable(bool CHI_UNUSED(enabled)) { }
#endif
