#include "runtime.h"
#include "stack.h"
#include "scavenger.h"
#include "thread.h"
#include "barrier.h"
#include "trace.h"
#include "copy.h"
#include "bit.h"
#include <chili/object/thunk.h>

typedef struct {
    ChiWorker*        worker;
    ChiHeapHandle     heapHandle;
    ChiBlockAlloc     *alloc, *cheneyAlloc;
    ChiBlockVec       dirty[CHI_GEN_MAX];
    ChiBlockVec       *grayList;
    ChiBlockVec       stack;
    ChiScavengerStats stats[CHI_STATS_SCAV_ENABLED];
    ChiGen            gcGen;
    uint32_t          blockGen, depth;
    uintptr_t         blockMask;
    size_t            blockWords;
    ChiColor          rawColor, objColor;
    bool              noPromote, noCollapse, addToGray, snapshot;
} ScavengerState;

CHI_INL void scavCount(ChiObjectCount* count, size_t words) {
    if (CHI_STATS_SCAV_ENABLED)
        chiCountObject(count, words);
}

static void scanObject(ScavengerState*, Chili);

static void scavMark(ScavengerState* state, Chili c) {
    CHI_ASSERT(chiMajor(c));
    CHI_ASSERT(chiReference(c));

    ChiType type = chiType(c);
    ChiObject* o = chiObject(c);

    if (chiRaw(type)) {
        ChiColor color = chiObjectColor(o);
        CHI_ASSERT(color != CHI_GRAY);
        if (color != CHI_BLACK)
            chiObjectSetColor(o, CHI_BLACK);
    } else if (chiObjectCasColor(o, CHI_WHITE, CHI_GRAY)) {
        chiBlockVecPush(state->grayList, c);
    }
}

CHI_INL Chili newMajorNormalObject(ScavengerState* state, ChiType type, size_t size) {
    /* Snapshot:
     * The new object must be marked black.
     * It must not be added to the gray list however, since
     * it will be scanned by the scavenger afterwards! The scavenger has to
     * scan the object since it has to fix all the references. This means
     * we are doing a part of the marking job in the scavenger at a snapshot.
     *
     * During marking (no snapshot):
     * Mark object gray since an already blacked dirty object could point to this
     * newly promoted major object. Otherwise the heap invariant would be violated
     * and the other black object would point to the new white one.
     * Since the object is being marked gray, it has to be added to the gray list.
     */
    Chili c = chiHeapNewSmall(&state->heapHandle, type, size, state->objColor);
    if (state->addToGray)
        chiBlockVecPush(state->grayList, c);
    return c;
}

CHI_INL Chili newMajorRawObject(ScavengerState* state, ChiType type, size_t size) {
    return chiHeapNewSmall(&state->heapHandle, type, size, state->rawColor);
}

CHI_INL Chili newRawObject(ScavengerState* state, ChiType type, size_t size, ChiGen gen) {
    CHI_ASSERT(gen < state->blockGen || gen == CHI_GEN_MAJOR);
    CHI_ASSERT(size <= CHI_MAX_UNPINNED);
    CHI_ASSERT(gen != CHI_GEN_NURSERY);
    return gen != CHI_GEN_MAJOR
        ? chiBlockNew(state->alloc + gen, type, size)
        : newMajorRawObject(state, type, size);
}

CHI_INL ChiGen promoteGen(ScavengerState* state, ChiGen gen, const ChiGen srcGen) {
    CHI_ASSERT(gen < state->blockGen);
    uint32_t nextGen = (uint32_t)gen + 1;
    if (nextGen == state->blockGen)
        return CHI_UNLIKELY(state->noPromote) ? gen : CHI_GEN_MAJOR;
    if (CHI_UNLIKELY(state->noPromote && srcGen == CHI_GEN_MAJOR))
        return (ChiGen)nextGen;
    // Eagerly promote objects referenced from older immutable objects.
    // We pass srcGen=NURSERY for mutable source objects!
    return (ChiGen)CHI_MAX(nextGen, srcGen);
}

CHI_INL bool forwarded(ScavengerState* state, void* payload) {
    ChiBlock* block = chiBlock(payload, state->blockMask);
    size_t idx = chiBlockIndex(block, payload);
    if (chiBitGet(block->marked, state->blockWords + idx))
        return true;
    chiBitSet(block->marked, state->blockWords + idx, true);
    return false;
}

CHI_INL Chili copyNormal(ScavengerState* state, Chili c, ChiType type, Chili* payload, ChiGen newGen) {
    Chili d;
    const size_t size = chiSize(c);
    if (newGen != CHI_GEN_MAJOR) {
        CHI_ASSERT(newGen != CHI_GEN_NURSERY);

        scavCount(&state->stats->object.copied, size);
        if (size != 1) {
            d = chiBlockNew(state->cheneyAlloc + newGen, type, size);
            Chili* dst = chiPayload(d);
            dst[0] = *payload;
            dst[1] = c;
            *payload = d;
            return d;
        }

        scavCount(&state->stats->object.copied1, size);
        d = chiBlockNew(state->alloc + newGen, type, size);
        *chiPayload(d) = *payload;
    } else {
        scavCount(&state->stats->object.promoted, size);
        d = newMajorNormalObject(state, type, size);
        chiCopyWords(chiPayload(d), payload, size);
    }
    *payload = d;

    // Scan recursively
    if (state->depth > 0) {
        --state->depth;
        scanObject(state, d);
        ++state->depth;
    } else {
        chiBlockVecPush(&state->stack, d);
    }
    return d;
}

CHI_INL Chili copyRaw(ScavengerState* state, Chili c, ChiType type, Chili* payload, ChiGen newGen) {
    size_t size = chiSize(c);
    const Chili d = newRawObject(state, type, size, newGen);
    chiCopyWords(chiRawPayload(d), payload, size);
    scavCount(newGen == CHI_GEN_MAJOR ? &state->stats->raw.promoted :
              &state->stats->raw.copied, size);
    *payload = d;
    return d;
}

CHI_INL ChiGen copy(ScavengerState* state, Chili* p, const ChiGen srcGen) {
    for (;;) {
        // Non-reference objects don't need copying (unboxed or empty objects)
        Chili c = *p;
        if (!chiReference(c))
            return CHI_GEN_MAJOR;

        // Only copy young objects for minor gc
        const ChiGen gen = chiGen(c, state->blockMask);
        if (gen > state->gcGen) {
            // Mark used objects in major heap referenced from minor heap.
            // This marking happens only at snapshot points,
            // when the whole minor heap is collected gcGen==genCount-1.
            // Basically the minor heap acts as black roots for the major heap.
            if (state->snapshot && gen == CHI_GEN_MAJOR)
               scavMark(state, c);
            return gen;
        }

        CHI_ASSERT(gen != CHI_GEN_MAJOR);

        // Collapse thunks, if the nested value is not a thunk itself.
        const ChiType type = chiType(c);
        if (!state->noCollapse && type == CHI_THUNK) {
            Chili clos;
            if (chiThunkCollapsible(c, &clos)) {
                if (CHI_STATS_SCAV_ENABLED)
                    ++state->stats->collapsed;
                *p = c = clos;
                continue;
            }
        }

        Chili* payload = (Chili*)chiRawPayload(c);
        if (forwarded(state, payload)) { // Replace reference with forwarding pointer, if already copied
            *p = *payload;
            return chiGen(*p, state->blockMask); // Return generation of forwarded object
        }

        ChiGen newGen = promoteGen(state, gen, srcGen);
        *p = !chiRaw(type)
            ? copyNormal(state, c, type, payload, newGen)
            : copyRaw(state, c, type, payload, newGen);
        return newGen;
    }
}

CHI_INL void scanMinorObject(ScavengerState* state, Chili c, Chili* orig, ChiGen gen, size_t size, Chili* payload, ChiBlock* block) {
    CHI_ASSERT(chiReference(c));

    const ChiType type = chiType(c);
    CHI_ASSERT(type <= CHI_LAST_IMMUTABLE
               || type == CHI_ARRAY || type == CHI_THREAD
               || type == CHI_THUNK || type == CHI_STRINGBUILDER);
    CHI_ASSERT(chiGen(c, state->blockMask) == gen);
    CHI_ASSERT(gen < state->blockGen); // Make coverity happy, why necessary? see scanMinorObjects

    const ChiGen srcGen = chiMutableType(type) ? CHI_GEN_NURSERY : gen;

    bool dirty = false;
    for (Chili* p = payload; p < payload + size; ++p) {
        if (p > payload)
            *p = *++orig;
        dirty |= copy(state, p, srcGen) < gen;
    }

    if (dirty) {
        size_t idx = chiBlockIndex(block, payload);
        chiBitSet(block->marked, idx, true);
        chiBlockVecPush(state->dirty + gen, c);
    }
}

static void scanObject(ScavengerState* state, Chili c) {
    CHI_ASSERT(chiReference(c));

    const ChiType type = chiType(c);
    CHI_ASSERT(type <= CHI_LAST_IMMUTABLE
                 || type == CHI_ARRAY || type == CHI_THREAD
                 || type == CHI_THUNK || type == CHI_STRINGBUILDER);

    const ChiGen gen = chiGen(c, state->blockMask);
    const ChiGen srcGen = chiMutableType(type) ? CHI_GEN_NURSERY : gen;

    Chili* payload = chiPayload(c);
    bool dirty = false;
    for (Chili* p = payload, *end = payload + chiSize(c); p < end; ++p)
        dirty |= copy(state, p, srcGen) < gen;

    if (gen == CHI_GEN_MAJOR) {
        chiObjectSetDirty(chiObject(c), dirty);
        if (dirty)
            chiBlockVecPush(state->dirty, c);
    } else {
        ChiBlock* block = chiBlock(payload, state->blockMask);
        size_t idx = chiBlockIndex(block, payload);
        chiBitSet(block->marked, idx, dirty);
        if (dirty)
            chiBlockVecPush(state->dirty + gen, c);
    }
}

CHI_INL void scanDirtyStack(ScavengerState* state, Chili c) {
    ChiStack* stack = chiToStack(c);
    uint32_t dirtyGen = CHI_GEN_MAJOR;
    for (Chili* p = stack->base; p < stack->sp; ++p)
        dirtyGen &= copy(state, p, CHI_GEN_NURSERY);
    if (dirtyGen != CHI_GEN_MAJOR)
        chiBlockVecPush(state->dirty, c);
    else
        chiObjectSetDirty(chiObject(c), false);
}

static void scanStackedObjects(ScavengerState* state) {
    Chili c;
    while (chiBlockVecPop(&state->stack, &c))
        scanObject(state, c);
    CHI_ASSERT(chiBlockVecNull(&state->stack));
}

static bool scanMinorObjects(ScavengerState* state) {
    bool done = true;
    for (uint32_t gen = state->blockGen; gen --> 1;) {
        CHI_FOREACH_BLOCK(b, &state->cheneyAlloc[gen].used) {
            while (b->base < b->ptr) {
                done = false;
                Chili* obj = (Chili*)b->base;
                Chili* orig = chiPayload(obj[1]);
                Chili c = orig[0];
                CHI_ASSERT(chiRawPayload(c) == obj);
                size_t size = chiSizeField(c);
                scanMinorObject(state, c, orig, (ChiGen)gen, size, obj, b);
                b->base += size;
            }
            CHI_ASSERT(b->base == b->ptr);
            if (b != state->cheneyAlloc[gen].block) {
                CHI_ASSERT(state->cheneyAlloc[gen].manager == state->alloc[gen].manager);
                chiBlockListDelete(&state->cheneyAlloc[gen].used, b);
                chiBlockListAppend(&state->alloc[gen].used, b);
            }
        }
    }
    return done;
}

static void scanCopiedObjects(ScavengerState* state) {
    do {
        scanStackedObjects(state);
    } while (!scanMinorObjects(state));
    for (uint32_t gen = state->blockGen; gen --> 1;) {
        chiBlockListDelete(&state->cheneyAlloc[gen].used, state->cheneyAlloc[gen].block);
        chiBlockListAppend(&state->alloc[gen].used, state->cheneyAlloc[gen].block);
        CHI_ASSERT(chiBlockListNull(&state->cheneyAlloc[gen].used));
        chiBlockAllocDestroy(state->cheneyAlloc + gen);
    }
    CHI_TT(state->worker, .name = "runtime;gc;scavenger;scan copied object");
}

static void scanDirty(ScavengerState* state, ChiRuntime* rt) {
    CHI_FOREACH_PROCESSOR (proc, rt) {
        // Free unused dirty lists
        for (size_t gen = 1; gen < state->gcGen + 1; ++gen)
            chiBlockVecFree(proc->gcPS.dirty + gen);

        // Scan dirty objects from tenured generations pointing to younger generations
        for (size_t gen = state->gcGen + 1; gen < state->blockGen; ++gen) {
            CHI_BLOCKVEC_FOREACH(c, proc->gcPS.dirty + gen) {
                scanObject(state, c);
                scavCount(&state->stats->dirty.object, chiSize(c));
            }
            chiBlockVecFree(proc->gcPS.dirty + gen);
        }

        // Scan dirty objects pointing from major heap to minor heap
        CHI_BLOCKVEC_FOREACH(c, proc->gcPS.dirty) {
            if (chiType(c) != CHI_STACK) {
                scanObject(state, c);
                scavCount(&state->stats->dirty.object, chiSize(c));
            }
        }
        CHI_TT(state->worker, .name = "runtime;gc;scavenger;scan dirty object");

        // Scan dirty stacks pointing from major heap to minor heap
        CHI_BLOCKVEC_FOREACH(c, proc->gcPS.dirty) {
            if (chiType(c) == CHI_STACK) {
                scanDirtyStack(state, c);
                scavCount(&state->stats->dirty.stack, chiSize(c));
            }
        }
        CHI_TT(state->worker, .name = "runtime;gc;scavenger;scan dirty stack");

        chiBlockVecFree(proc->gcPS.dirty);
    }
}

static void copyRoots(ScavengerState* state, ChiRuntime* rt) {
    // Copy processor roots finally. They will only be really copied
    // and scanned if the haven't been yet. Otherwise they
    // are just replaced with the forward pointer (or remain the same for pinned)
    CHI_FOREACH_PROCESSOR (proc, rt) {
        copy(state, &proc->thread, CHI_GEN_NURSERY);
        copy(state, &proc->schedulerThread, CHI_GEN_NURSERY);
    }
}

static void prepareAllocs(ScavengerState* state, ChiTenure* tenure) {
    for (uint32_t gen = state->blockGen; gen --> 1;)
        chiBlockAllocSetup(state->cheneyAlloc + gen, (ChiGen)gen, &tenure->manager);
}

static void prepareDirtyLists(ScavengerState* state, ChiBlockManager* bm) {
    for (uint32_t gen = state->blockGen; gen --> 0;)
        chiBlockVecInit(state->dirty + gen, bm);
}

static void saveDirtyLists(ScavengerState* state, ChiBlockVec* dirty) {
    for (uint32_t gen = state->blockGen; gen --> 0;)
        chiBlockVecJoin(dirty + gen, state->dirty + gen);
}

static void blockScav(ChiBlockAlloc* ba, bool begin, bool fresh) {
    if (begin) {
        chiBlockListJoin(&ba->old, &ba->used);
        ba->block = 0;
    } else {
        chiBlockManagerFreeList(ba->manager, &ba->old);
    }
    if (fresh)
        chiBlockFresh(ba);
}

static void blockScavenger(ChiRuntime* rt, ChiGen gcGen, bool begin) {
    for (uint32_t gen = 0; gen < gcGen; ++gen)
        blockScav(rt->tenure.alloc + gen, begin, begin);
    CHI_FOREACH_PROCESSOR(p, rt) {
        blockScav(&p->nursery.bumpAlloc, begin, !begin);
        blockScav(&p->nursery.outOfBandAlloc, begin, !begin);
    }
}

void chiScavenger(ChiProcessor* proc, ChiGen gcGen, bool snapshot, ChiScavengerStats* stats) {
    ChiRuntime* rt = proc->rt;
    const ChiRuntimeOption* opt = &rt->option;

    ChiColor objColor = snapshot ? CHI_BLACK : rt->gc.msPhase == CHI_MS_PHASE_MARK ? CHI_GRAY : CHI_WHITE;
    ScavengerState state = {
        .worker     = &proc->worker,
        CHI_IF(CHI_STATS_SCAV_ENABLED, .stats[0] = { .gen = gcGen, .snapshot = snapshot },)
        .gcGen   = gcGen,
        .snapshot = snapshot,
        // -1 since gen 0 is unused, cast to pointer - otherwise ubsan reports out of bounds
        .cheneyAlloc = (ChiBlockAlloc*)rt->tenure.cheneyAlloc - 1,
        .alloc      = (ChiBlockAlloc*)rt->tenure.alloc - 1,
        .blockWords = opt->block.size / CHI_WORDSIZE,
        .blockMask  = CHI_MASK(opt->block.size),
        .blockGen   = opt->block.gen,
        .grayList   = &proc->gcPS.grayList,
        .noPromote  = opt->gc.scav.noPromote,
        .depth      = opt->heap.scanDepth,
        // don't collapse during mark such that the color invariant will not be violated
        // otherwise an already blackend object could be modified by collapse,
        // pointing to a white object afterwards
        .noCollapse = opt->gc.scav.noCollapse || rt->gc.msPhase == CHI_MS_PHASE_MARK,
        .addToGray  = objColor == CHI_GRAY,
        .rawColor   = rt->gc.msPhase == CHI_MS_PHASE_MARK ? CHI_BLACK : CHI_WHITE,
        .objColor   = objColor,
    };

    chiHeapHandleInit(&state.heapHandle, &rt->heap);
    chiBlockVecInit(&state.stack, &rt->tenure.manager);

    blockScavenger(rt, gcGen, true);
    prepareDirtyLists(&state, &rt->tenure.manager);
    prepareAllocs(&state, &rt->tenure);
    scanDirty(&state, rt);
    copyRoots(&state, rt);
    scanCopiedObjects(&state);
    saveDirtyLists(&state, proc->gcPS.dirty);
    blockScavenger(rt, gcGen, false);
    chiHeapHandleRelease(&state.heapHandle);

    *stats = CHI_STATS_SCAV_ENABLED ? *state.stats : (ChiScavengerStats){};
}
