#include "barrier.h"
#include "copy.h"
#include "stack.h"
#include "tracepoint.h"

typedef struct {
    ChiProcessor*     proc;
    ChiHeapHandle*    heapHandle;
    ChiBlockList      oldList;
    ChiBlockAlloc     *agedAlloc;
    ChiBlockAlloc     cheneyAlloc;
    ChiBlockVec       majorDirty;
    ChiLocalGC        *gc;
    ChiBlockVec       stack;
    CHI_IF(CHI_COUNT_ENABLED, ChiScavengerStats stats;)
    uint32_t          aging, depth;
    uintptr_t         blockMask;
    size_t            blockSize;
    bool              collapse;
    ChiColor          black;
} ScavengerState;

static void scanMajorObject(ScavengerState*, Chili);
static void copyMajorObject(ScavengerState*, Chili, Chili*);

CHI_INL Chili copyNormal(ScavengerState* state, ChiType type, Chili* payload, size_t size, ChiGen newGen) {
    void* dstPayload;
    Chili c;

    if (newGen != CHI_GEN_MAJOR) {
        CHI_ASSERT(newGen != CHI_GEN_NURSERY);
        chiCountObject(state->stats.object.copied, size);
        dstPayload = chiBlockAllocNew(&state->cheneyAlloc, size);
        c = chiWrap(dstPayload, size, type, newGen);
    } else {
        chiCountObject(state->stats.object.promoted, size);
        dstPayload = chiHeapHandleNewSmall(state->heapHandle, size, CHI_NEW_LOCAL);
        c = chiWrap(dstPayload, size, type, newGen);
        if (state->depth > 0) {
            --state->depth;
            copyMajorObject(state, c, payload); // scan recursively
            ++state->depth;
            return c;
        }
        chiBlockVecPush(&state->stack, c);
    }

    chiCopyWords(dstPayload, payload, size);
    return *payload = c;
}

CHI_INL Chili copyRaw(ScavengerState* state, ChiType type, Chili* payload, size_t size, ChiGen newGen) {
    CHI_ASSERT(newGen < state->aging || newGen == CHI_GEN_MAJOR);
    CHI_ASSERT(size <= CHI_MAX_UNPINNED);
    CHI_ASSERT(newGen != CHI_GEN_NURSERY);

    void* dstPayload = newGen != CHI_GEN_MAJOR
        ? chiBlockAllocNew(state->agedAlloc, size)
        : chiHeapHandleNewSmall(state->heapHandle, size, CHI_NEW_SHARED | CHI_NEW_CLEAN);

    chiCopyWords(dstPayload, payload, size);

    if (newGen == CHI_GEN_MAJOR)
        chiCountObject(state->stats.raw.promoted, size);
    else
        chiCountObject(state->stats.raw.copied, size);

    return *payload = chiWrap(dstPayload, size, type, newGen);
}

CHI_INL ChiGen copy(ScavengerState* state, Chili* p, bool collapse) {
    Chili c = *p;
    for (;;) {
        // Non-reference objects don't need copying (unboxed or empty objects)
        if (chiUnboxed(c))
            return CHI_GEN_MAJOR;

        // Only copy young objects
        const ChiGen gen = chiGen(c);
        if (gen == CHI_GEN_MAJOR) {
            // Mark used objects in major heap referenced from minor heap.
            // This marking happens only at snapshot points,
            // when the whole minor heap is collected gen==genCount-1.
            // Basically the minor heap acts as black roots for the major heap.
            if (state->gc)
                chiGrayMark(state->gc, c);
            return CHI_GEN_MAJOR;
        }

        // Collapse thunks, if the nested value is not a thunk itself.
        const ChiType type = chiType(c);
        if (collapse && type == CHI_THUNK && state->collapse) {
            Chili collapsed = chiFieldRead(&chiToThunk(c)->val);
            if (chiThunkCollapsible(collapsed)) {
                chiCount(state->stats.collapsed, 1);
                *p = c = collapsed;
                continue;
            }
        }

        Chili* payload = (Chili*)chiRawPayload(c);
        if (!chiBlockAllocSetForwardMasked(payload, state->blockMask)) {
            // Replace reference with forwarding pointer, if already copied
            *p = *payload;
            return chiGen(*p); // Return generation of forwarded object
        }

        ChiGen newGen = (ChiGen)((uint32_t)gen + 1);
        if (newGen >= state->aging)
            newGen = CHI_GEN_MAJOR;

        size_t size = chiSize(c);
        *p = chiRaw(type)
            ? copyRaw(state, type, payload, size, newGen)
            : copyNormal(state, type, payload, size, newGen);
        return newGen;
    }
}

static CHI_NOINL void copyMajorObject(ScavengerState* state, Chili c, Chili* src) {
    CHI_ASSERT(chiType(c) != CHI_STACK);
    CHI_ASSERT(chiType(c) != CHI_THREAD);
    CHI_ASSERT(!chiRaw(chiType(c)));

    ChiObject* obj = chiObject(c);
    CHI_ASSERT(!chiObjectShared(obj));
    CHI_ASSERT(!state->gc || chiColorEq(chiObjectColor(obj), state->black));

    bool dirty = false;
    for (Chili* p = chiPayload(c), *end = p + chiSize(c), *q = src; p < end; ++p, ++q) {
        *p = *q;
        if (src == q) *q = c; // forward pointer
        dirty |= copy(state, p, false) < CHI_GEN_MAJOR;
    }

    CHI_ASSERT(state->aging != 1 || !dirty);
    chiObjectSetDirty(obj, dirty);
    if (dirty)
        chiBlockVecPush(&state->majorDirty, c);
}

static CHI_NOINL void scanMajorObject(ScavengerState* state, Chili c) {
    CHI_ASSERT(chiType(c) != CHI_STACK);
    CHI_ASSERT(chiType(c) != CHI_THREAD);
    CHI_ASSERT(!chiRaw(chiType(c)));

    ChiObject* obj = chiObject(c);
    CHI_ASSERT(!chiObjectShared(obj));

    if (state->gc)
        chiObjectSetColor(obj, state->black);

    bool dirty = false;
    for (Chili* p = chiPayload(c), *end = p + chiSize(c); p < end; ++p)
        dirty |= copy(state, p, false) < CHI_GEN_MAJOR;

    CHI_ASSERT(state->aging != 1 || !dirty);
    chiObjectSetDirty(obj, dirty);
    if (dirty)
        chiBlockVecPush(&state->majorDirty, c);
}

static void scanDirtyStack(ScavengerState* state, Chili c) {
    CHI_ASSERT(chiGen(c) == CHI_GEN_MAJOR);

    ChiObject* obj = chiObject(c);
    CHI_ASSERT(!chiObjectShared(obj));

    CHI_LOCK_OBJECT(obj);
    CHI_ASSERT(atomic_load_explicit(&state->proc->gc.phase, memory_order_relaxed) != CHI_GC_ASYNC
               || chiColorEq(chiObjectColor(obj), state->black));

    if (state->gc)
        chiObjectSetColor(obj, state->black);

    ChiStack* s = chiToStack(c);
    CHI_ASSERT(chiObjectDirty(obj));

    bool dirty = false;
    chiCountObject(state->stats.dirty.stacks, (size_t)(s->sp - s->base));
    for (Chili* p = s->base; p < s->sp; ++p)
        dirty |= copy(state, p, false) < CHI_GEN_MAJOR;

    CHI_ASSERT(state->aging != 1 || !dirty);
    chiObjectSetDirty(obj, dirty);
    if (dirty)
        chiBlockVecPush(&state->majorDirty, c);
}

static void scanPostponed(ScavengerState* state) {
    Chili c;
    while (chiBlockVecPop(&state->stack, &c))
        scanMajorObject(state, c);
}

static bool scanCheneyObjects(ScavengerState* state) {
    bool done = true;
    CHI_FOREACH_BLOCK(b, &state->cheneyAlloc.usedList) {
        if (b->alloc.base < b->alloc.ptr)
            done = false;
        while (b->alloc.base < b->alloc.ptr) {
            copy(state, (Chili*)b->alloc.base, true);
            ++b->alloc.base;
        }
        CHI_ASSERT(b->alloc.base == b->alloc.ptr);
        if (b != state->cheneyAlloc.block) {
            b->alloc.base = chiBlockAllocBase(b, state->blockSize);
            CHI_ASSERT(state->cheneyAlloc.manager == state->agedAlloc->manager);
            chiBlockListDelete(&state->cheneyAlloc.usedList, b);
            chiBlockListAppend(&state->agedAlloc->usedList, b);
        }
    }
    return done;
}

static void scanCopiedObjects(ScavengerState* state) {
    do {
        scanPostponed(state);
    } while (!scanCheneyObjects(state));

    ChiBlock* b = state->cheneyAlloc.block;
    b->alloc.base = chiBlockAllocBase(b, state->blockSize);
    chiBlockListDelete(&state->cheneyAlloc.usedList, b);
    chiBlockListAppend(&state->agedAlloc->usedList, b);
    CHI_ASSERT(chiBlockListNull(&state->cheneyAlloc.usedList));
    chiBlockAllocDestroy(&state->cheneyAlloc);

    CHI_TTP(state->proc, .name = "runtime;gc;scavenger;scan");
}

static void scanDirty(ScavengerState* state, ChiBlockVec* dirtyVec) {
    Chili c;
    while (chiBlockVecPop(dirtyVec, &c)) {
        ChiObject* obj = chiObject(c);
        if (chiType(c) == CHI_STACK) {
            scanDirtyStack(state, c);
        } else if (!chiObjectShared(obj)) { // Check if object has been promoted to shared heap
            CHI_ASSERT(chiObjectDirty(obj));
            scanMajorObject(state, c);
            chiCountObject(state->stats.dirty.major, chiSize(c));
        }
    }
    CHI_TTP(state->proc, .name = "runtime;gc;scavenger;dirty");
}

static void saveDirtyLists(ScavengerState* state, ChiLocalHeap* h) {
    chiBlockVecJoin(&h->majorDirty, &state->majorDirty);
    CHI_ASSERT(chiBlockVecNull(&h->majorDirty) || state->aging != 1);
}

static void joinOld(ScavengerState* state, ChiBlockAlloc* ba) {
    chiBlockListJoin(&state->oldList, &ba->usedList);
    ba->block = 0;
}

static void moveOldBlocks(ScavengerState* state, ChiLocalHeap* h) {
    chiBlockListInit(&state->oldList);
    joinOld(state, &h->nursery.alloc);
    joinOld(state, &h->agedAlloc);
    chiBlockAllocFresh(&h->agedAlloc);
}

static void freeOldBlocks(ScavengerState* state, ChiLocalHeap* h) {
    h->nursery.bump = chiBlockAllocTake(&h->nursery.alloc);
    chiBlockAllocFresh(&h->nursery.alloc);
    chiBlockManagerFreeList(&h->manager, &state->oldList);
}

static void forwardPromoted(ChiPromotedVec* promoted) {
    CHI_BLOCKVEC_FOREACH(ChiPromotedVec, p, promoted) {
        CHI_ASSERT(chiGen(p->from) != CHI_GEN_MAJOR);
        CHI_ASSERT(chiGen(p->to) == CHI_GEN_MAJOR);
        *(Chili*)chiRawPayload(p->from) = p->to;
    }
    chiPromotedVecFree(promoted);
}

void chiScavenger(ChiProcessor* proc, uint32_t aging, bool snapshot, ChiScavengerStats* stats) {
    ChiRuntime* rt = proc->rt;
    ChiLocalHeap* h = &proc->heap;
    const ChiRuntimeOption* opt = &rt->option;

    ScavengerState state = {
        CHI_IF(CHI_COUNT_ENABLED, .stats = {
            .snapshot = snapshot,
            .aging = aging,
        },)
        .proc       = proc,
        .agedAlloc  = &h->agedAlloc,
        .blockSize  = opt->block.size,
        .blockMask  = CHI_ALIGNMASK(opt->block.size),
        .aging      = aging,
        .gc         = snapshot ? &proc->gc : 0,
        .black      = proc->gc.markState.black,
        .depth      = opt->heap.scanDepth,
        .collapse   = !opt->gc.scav.noCollapse,
        .heapHandle = &proc->handle,
    };

    chiBlockVecInit(&state.stack, &h->manager);
    chiBlockVecInit(&state.majorDirty, &h->manager);
    chiBlockAllocSetup(&state.cheneyAlloc, &h->manager, 0);
    moveOldBlocks(&state, h);
    forwardPromoted(&h->promoted.vec);
    scanDirty(&state, &h->majorDirty);
    scanCopiedObjects(&state);
    saveDirtyLists(&state, &proc->heap);
    freeOldBlocks(&state, h);
    CHI_CHOICE(CHI_COUNT_ENABLED, *stats = state.stats, CHI_NOWARN_UNUSED(stats));
}
