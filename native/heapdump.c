#include "bigint.h"
#include "blockman.h"
#include "mem.h"
#include "heapdump.h"
#include "sink.h"
#include "location.h"
#include "runtime.h"
#include "strutil.h"
#include "set.h"
#include "stack.h"
#include "thread.h"
#include "chunk.h"
#include <chili/object/buffer.h>
#include <chili/object/thunk.h>

typedef struct  {
    ChiLoc loc;
    size_t id;
} LocRecord;

#define NO_WID (~0U)

#define HASH        LocHash
#define ENTRY       LocRecord
#define PREFIX      locHash
#define HASHFN(k)   chiHashPtr(k->id)
#define EXISTS(e)   e->loc.id
#define KEY(e)      (const ChiLoc*)&(e->loc)
#define KEYEQ(a, b) memeq(a, b, sizeof (ChiLoc))
#include "hashtable.h"

#define S_ELEM       LocRecord
#define S_LESS(a, b) ((a)->id < (b)->id)
#include "sort.h"

typedef struct {
    ChiBlockVec  stack;
    Set          visited;
    LocHash      locHash;
    ChiSink*     sink;
    uintptr_t    blockMask;
    ChiBigIntBuf bigIntBuf;
} DumpState;

static void dumpSep(ChiSink* sink, bool* first) {
    if (*first)
        *first = false;
    else
        chiSinkPuts(sink, ",\n");
}

static void dumpObjectId(ChiSink* sink, Chili c) {
    chiSinkFmt(sink, "%qzx", chiAddress(c));
}

static uintptr_t ptrOffset(void* p) {
    return ((uintptr_t)p - CHI_CHUNK_START) / CHI_WORDSIZE;
}

static void dumpChili(DumpState* state, Chili c) {
    ChiSink* sink = state->sink;
    if (chiUnboxed(c)) {
        chiSinkFmt(sink, "\"U%jx\"", chiToUnboxed(c));
    } else if (chiEmpty(c)) {
        ChiType type = chiType(c);
        if (type <= CHI_LAST_TAG)
            chiSinkFmt(sink, "\"EMPTY_DATA%u\"", type);
        else
            chiSinkFmt(sink, "\"EMPTY_%s\"", chiTypeName(type));
    } else {
        dumpObjectId(sink, c);
        if (setInserted(&state->visited, c))
            chiBlockVecPush(&state->stack, c);
    }
}

static void dumpField(DumpState* state, const char* name, Chili c) {
    chiSinkFmt(state->sink, ",\"%s\":", name);
    dumpChili(state, c);
}

static void dumpLocId(DumpState* state, ChiLoc loc) {
    LocRecord* r;
    if (locHashCreate(&state->locHash, &loc, &r)) {
        r->loc = loc;
        r->id = state->locHash.used - 1;
    }
    chiSinkFmt(state->sink, ",\"loc\":%zu", r->id);
}

static void dumpBuffer(ChiSink* sink, const void* buf, size_t size) {
    chiSinkFmt(sink, ",\"payload\":%hb", (uint32_t)size, buf);
}

static void dumpStack(DumpState* state, ChiStack* stack) {
#define _FRAME_NAME(n) CHI_STRINGIZE(n),
    static const char* const frameName[] = { CHI_FOREACH_FRAME(_FRAME_NAME) };

    ChiSink* sink = state->sink;
    if (stack->sp - 1 < stack->base) {
        chiSinkPuts(sink, ",\"frames\":[]");
        return;
    }
    chiSinkPuts(sink, ",\"frames\":\n[");
    for (Chili* p = stack->sp - 1; p >= stack->base; ) {
        if (p < stack->sp - 1)
            chiSinkPuts(sink, "\n,");
        chiSinkFmt(sink, "{\"frame\":\"%s\"", frameName[chiFrame(p)]);
        dumpLocId(state, chiLocateFrame(p));
        chiSinkPuts(sink, ",\"payload\":[");
        for (size_t size = chiFrameSize(p), i = 0; i < size; ++i) {
            if (i > 0)
                chiSinkPutc(sink, ',');
            dumpChili(state, *p--);
        }
        chiSinkPuts(sink, "]}");
    }
    chiSinkPutc(sink, ']');
}

static void dumpObject(DumpState* state, Chili c) {
    ChiSink* sink = state->sink;
    const ChiType type = chiType(c);

    dumpObjectId(sink, c);
    if (chiFn(type))
        chiSinkFmt(sink, ":{\"type\":\"FN\",\"arity\":%u", chiFnArity(c));
    else if (type <= CHI_LAST_TAG)
        chiSinkFmt(sink, ":{\"type\":\"DATA\",\"tag\":%u", type);
    else
        chiSinkFmt(sink, ":{\"type\":\"%s\"", chiTypeName(type));
    size_t size = chiSize(c);
    chiSinkFmt(sink, ",\"gen\":%u,\"size\":%zu", chiGen(c, state->blockMask), size);

    switch (type) {
    case CHI_STRINGBUILDER: {
        ChiStringBuilder* b = chiToStringBuilder(c);
        uint32_t n = chiToUInt32(b->size);
        // String is in an intermediate state during build, therefore access contents directly
        chiSinkFmt(sink, ",\"size\":%u,\"string\":%qb", n, n, chiRawPayload(chiAtomicLoad(&b->string)));
        break;
    }
    case CHI_THREAD: {
        ChiThread* t = chiToThread(c);
        dumpField(state, "name", chiThreadName(c));
        dumpField(state, "stack", chiAtomicLoad(&t->stack));
        dumpField(state, "state", chiThreadState(c));
        dumpField(state, "local", chiThreadLocal(c));
        dumpField(state, "tid", t->tid);
        dumpField(state, "exceptBlock", t->exceptBlock);
        break;
    }
    case CHI_THUNK:
        dumpField(state, "value", chiAtomicLoad(&chiToThunk(c)->val));
        break;
    case CHI_STACK:
        dumpStack(state, chiToStack(c));
        break;
    case CHI_BIGINT:
        chiSinkFmt(sink, ",\"int\":\"%s\"", chiBigIntStr(c, &state->bigIntBuf));
        break;
    case CHI_RAW:
    case CBY_FFI:
        dumpBuffer(sink, chiRawPayload(c), CHI_WORDSIZE * size);
        break;
    case CHI_BUFFER:
        dumpBuffer(sink, chiBufferBytes(c), chiBufferSize(c));
        break;
    case CHI_STRING:
        chiSinkFmt(sink, ",\"string\":%qS", chiStringRef(&c));
        break;
    case CHI_BOX64:
        chiSinkFmt(sink, ",\"value\":%qjx", chiUnbox64(c));
        break;
    case CHI_ARRAY:
    case CHI_FIRST_TAG...CHI_LAST_IMMUTABLE:
        if ((chiFn(type) || type == CHI_THUNKFN))
            dumpLocId(state, chiLocateFn(c));
        chiSinkPuts(sink, ",\"payload\":[");
        for (Chili* payload = chiPayload(c), *p = payload, *end = p + size; p < end; ++p) {
            if (p > payload)
                chiSinkPutc(sink, ',');
            dumpChili(state, *p);
        }
        chiSinkPutc(sink, ']');
        break;
    }
    chiSinkPutc(sink, '}');
}

static void dumpBlocks(ChiSink* sink, bool* first, const ChiBlockList* list, ChiGen gen, uint32_t pid) {
    CHI_FOREACH_BLOCK_NODELETE(block, list) {
        dumpSep(sink, first);
        chiSinkFmt(sink, "{\"addr\":%qzx,\"ptr\":%qzx,\"gen\":%u",
                    ptrOffset(block), ptrOffset(block->ptr), gen);
        if (pid != NO_WID)
            chiSinkFmt(sink, ",\"pid\":%u", pid);
        chiSinkPutc(sink, '}');
    }
}

static void dumpChunk(ChiSink* sink, const char* type, const ChiChunk* chunk, uint32_t pid) {
    chiSinkFmt(sink, "{\"type\":\"%s\",\"addr\":%qzx,\"size\":%qzu",
                type, ptrOffset(chunk->start), chunk->size / CHI_WORDSIZE);
    if (pid != NO_WID)
        chiSinkFmt(sink, ",\"pid\":%u", pid);
    chiSinkPutc(sink, '}');
}

static void dumpChunks(ChiSink* sink, bool* first, const char* type, const ChiList* list, uint32_t pid) {
    CHI_FOREACH_CHUNK_NODELETE(chunk, list) {
        dumpSep(sink, first);
        dumpChunk(sink, type, chunk, pid);
    }
}

static void dumpHeapChunks(ChiSink* sink, bool* first, ChiHeap* heap) {
    static const char* const className[] = { "Small", "Medium" };
    static const char* const zoneName[] = { "Free", "Full", "SweptFree", "SweptFull", "Sweep", "Used" };

    CHI_LOCK(&heap->mutex);
    dumpChunks(sink, first, "heapLarge", &heap->large.list, NO_WID);
    dumpChunks(sink, first, "heapLargeSwept", &heap->large.listSwept, NO_WID);

    for (uint32_t i = 0; i < CHI_DIM(heap->cls); ++i) {
        for (uint32_t j = 0; j < CHI_DIM(heap->cls[i].izone); ++j) {
            char type[64];
            chiFmt(type, sizeof (type), "heap%s%s", className[i], zoneName[j]);
            CHI_FOREACH_HEAP_SEGMENT_NODELETE(s, heap->cls[i].izone + j) {
                dumpSep(sink, first);
                dumpChunk(sink, type, s->chunk, NO_WID);
            }
        }
    }
}

void chiHeapDump(ChiRuntime* rt, ChiSink* sink) {
    const ChiRuntimeOption* opt = &rt->option;

    DumpState state = {
        .sink = sink,
        .blockMask = CHI_MASK(opt->block.size),
    };
    chiBlockVecInit(&state.stack, &rt->blockTemp);

    chiSinkFmt(sink, "{\"ts\":%qju\n,\"blockSize\":%zu\n,\"blocks\":\n[",
                    CHI_UN(Nanos, chiClock(CHI_CLOCK_REAL_FINE)), rt->option.block.size / CHI_WORDSIZE);

    bool first = true;
    for (uint32_t gen = 1; gen < opt->block.gen; ++gen)
        dumpBlocks(sink, &first, &rt->tenure.alloc[gen - 1].used, (ChiGen)gen, NO_WID);

    CHI_FOREACH_PROCESSOR (proc, rt) {
        dumpBlocks(sink, &first, &proc->nursery.bumpAlloc.used, CHI_GEN_NURSERY, proc->worker.wid);
        dumpBlocks(sink, &first, &proc->nursery.outOfBandAlloc.used, CHI_GEN_NURSERY, proc->worker.wid);
    }

    chiSinkPuts(sink, "]\n,\"chunks\":\n[");

    first = true;
    dumpChunks(sink, &first, "minorTenure", &rt->tenure.manager.chunkList, NO_WID);
    CHI_FOREACH_PROCESSOR (proc, rt)
        dumpChunks(sink, &first, "minorNursery", &proc->nursery.manager.chunkList, proc->worker.wid);
    dumpChunks(sink, &first, "blockTemp", &rt->blockTemp.chunkList, NO_WID);
    dumpHeapChunks(sink, &first, &rt->heap);

    chiSinkPuts(sink, "]\n,\"roots\":\n[");

    first = true;
    for (size_t i = 0; i < rt->gc.roots.used; ++i) {
        dumpSep(sink, &first);
        dumpChili(&state, rt->gc.roots.elem[i]);
    }

    chiSinkPuts(sink, "]\n,\"processors\":\n[");

    first = true;
    CHI_FOREACH_PROCESSOR (proc, rt) {
        dumpSep(sink, &first);
        chiSinkFmt(sink, "{\"pid\":%u,\"name\":%qs", proc->worker.wid, proc->worker.name);
        dumpField(&state, "thread", proc->thread);
        dumpField(&state, "schedulerThread", proc->schedulerThread);
        chiSinkPutc(sink, '}');
    }
    chiSinkPuts(sink, "]\n,\"objects\":\n{");

    Chili c;
    first = true;
    while (chiBlockVecPop(&state.stack, &c)) {
        dumpSep(sink, &first);
        dumpObject(&state, c);
    }

    chiSinkPuts(sink, "},\n\"locations\":\n[");

    sortLocRecord(state.locHash.entry, state.locHash.capacity);
    first = true;
    HASH_FOREACH(locHash, r, &state.locHash) {
        dumpSep(sink, &first);
        ChiLocLookup lookup;
        chiLocLookup(&lookup, r->loc);
        chiSinkFmt(sink, "{\"fn\":%qS,\"interp\":%s",
                    lookup.loc.fn, lookup.loc.interp ? "true" : "false");
        if (lookup.loc.module.size)
            chiSinkFmt(sink, ",\"module\":%qS", lookup.loc.module);
        if (lookup.loc.file.size)
            chiSinkFmt(sink, ",\"file\":%qS,\"line\":%u", lookup.loc.file, lookup.loc.line);
        chiSinkPutc(sink, '}');
    }

    chiSinkPuts(sink, "]}\n");
    locHashDestroy(&state.locHash);
    setDestroy(&state.visited);
    chiFree(state.bigIntBuf.buf);
}
