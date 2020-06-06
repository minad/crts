
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
    ChiObjVec         dirtyObjects;
    ChiLocalGC        *gc;
    ChiObjVec         majorObjects;
    CHI_IF(CHI_COUNT_ENABLED, ChiScavengerStats stats;)
    uint32_t          aging;
    uintptr_t         blockMask;
    size_t            blockSize;
    ChiColor          black;
} ScavengerState;

CHI_INL bool copy(ScavengerState* state, Chili* p, bool collapse) {
    Chili c = *p;
    for (;;) {
        // Non-reference objects don't need copying (unboxed or empty objects)
        if (!chiRef(c))
            return false;

        // Only copy young objects
        const ChiGen gen = chiGen(c);
        if (gen == CHI_GEN_MAJOR) {
            // Mark used objects in major heap referenced from minor heap.
            // This marking happens only at snapshot points,
            // when the whole minor heap is collected gen==genCount-1.
            // Basically the minor heap acts as black roots for the major heap.
            if (state->gc)
                chiMarkRef(state->gc, c);
            return false;
        }

        const ChiType type = chiType(c);
        Chili* const oldPayload = (Chili*)chiRawPayload(c);
        const ChiGen newGen = gen >= state->aging ? CHI_GEN_MAJOR : (ChiGen)((uint32_t)gen + 1);
        const size_t size = chiSizeSmall(c);
        void* newPayload;
        Chili d;

        if (chiRaw(type)) {
            if (!chiBlockAllocSetForwardMasked(oldPayload, state->blockMask)) {
                // Replace reference with forwarding pointer, if already copied
                *p = *oldPayload;
                return chiGen(*p) != CHI_GEN_MAJOR;
            }

            if (newGen != CHI_GEN_MAJOR) {
                chiCountObject(state->stats.raw.copied, size);
                newPayload = chiBlockAllocNew(state->agedAlloc, size);
            } else {
                chiCountObject(state->stats.raw.promoted, size);
                newPayload = chiHeapHandleNewSmall(state->heapHandle, size, CHI_NEW_SHARED | CHI_NEW_CLEAN);
            }
            d = chiWrap(newPayload, size, type, newGen);
        } else {
            // Collapse thunks, if the nested value is not a thunk itself.
            Chili collapsed;
            if (CHI_SCAV_COLLAPSE_ENABLED &&
                collapse && type == CHI_THUNK &&
                chiThunkCollapsible(chiToThunk(c), &collapsed)) {
                chiCount(state->stats.collapsed, 1);
                *p = c = collapsed;
                continue;
            }

            if (!chiBlockAllocSetForwardMasked(oldPayload, state->blockMask)) {
                // Replace reference with forwarding pointer, if already copied
                *p = *oldPayload;
                return chiGen(*p) != CHI_GEN_MAJOR;
            }

            if (newGen != CHI_GEN_MAJOR) {
                chiCountObject(state->stats.object.copied, size);
                newPayload = chiBlockAllocNew(&state->cheneyAlloc, size);
                d = chiWrap(newPayload, size, type, newGen);
            } else {
                chiCountObject(state->stats.object.promoted, size);
                newPayload = chiHeapHandleNewSmall(state->heapHandle, size, CHI_NEW_LOCAL);
                d = chiWrap(newPayload, size, type, newGen);
                chiObjVecPush(&state->majorObjects, d);
            }
        }

        chiCopyWords(newPayload, oldPayload, size);
        *p = *oldPayload = d;
        return newGen != CHI_GEN_MAJOR;
    }
}

CHI_INL void scanMajorObject(ScavengerState* state, Chili c) {
    CHI_ASSERT(chiType(c) != CHI_STACK);
    CHI_ASSERT(chiType(c) != CHI_THREAD);
    CHI_ASSERT(!chiRaw(chiType(c)));

    ChiObject* obj = chiObject(c);
    CHI_ASSERT(!chiObjectShared(obj));
    CHI_ASSERT(obj->flags.dirty);

    if (state->gc)
        chiObjectSetColor(obj, state->black);

    bool dirty = false;
    for (Chili* p = (Chili*)obj->payload, *end = p + obj->size; p < end; ++p)
        dirty |= copy(state, p, false);

    CHI_ASSERT(state->aging || !dirty);
    if (dirty)
        chiObjVecPush(&state->dirtyObjects, c);
    else
        obj->flags.dirty = false;
}

static void scanDirtyStack(ScavengerState* state, Chili c) {
    ChiObject* obj = chiObject(c);
    CHI_ASSERT(!chiObjectShared(obj));
    CHI_ASSERT(obj->flags.dirty);

    CHI_LOCK_OBJECT(obj);
    CHI_ASSERT(atomic_load_explicit(&state->proc->gc.phase, memory_order_relaxed) != CHI_GC_ASYNC
               || chiColorEq(chiObjectColor(obj), state->black));

    if (state->gc)
        chiObjectSetColor(obj, state->black);

    ChiStack* s = (ChiStack*)obj->payload;
    chiCountObject(state->stats.dirty.stack, (size_t)(s->sp - s->base));
    bool dirty = false;
    for (Chili* p = s->base; p < s->sp; ++p)
        dirty |= copy(state, p, false);

    CHI_ASSERT(state->aging || !dirty);
    obj->flags.dirty = dirty;
    if (dirty)
        chiObjVecPush(&state->dirtyObjects, c);
}

static void scanMajorObjectList(ScavengerState* state) {
    Chili c;
    while (chiObjVecPop(&state->majorObjects, &c))
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
        scanMajorObjectList(state);
    }
    return done;
}

static void scanCopiedObjects(ScavengerState* state) {
    while (!scanCheneyObjects(state));
    chiObjVecFree(&state->majorObjects);

    ChiBlock* b = state->cheneyAlloc.block;
    b->alloc.base = chiBlockAllocBase(b, state->blockSize);
    chiBlockListDelete(&state->cheneyAlloc.usedList, b);
    chiBlockListAppend(&state->agedAlloc->usedList, b);
    CHI_ASSERT(chiBlockListNull(&state->cheneyAlloc.usedList));
    chiBlockAllocDestroy(&state->cheneyAlloc);

    CHI_TTP(state->proc, .name = "runtime;gc;scavenger;scan");
}

static void scanDirtyObjects(ScavengerState* state, ChiObjVec* dirtyVec) {
    Chili c;
    while (chiObjVecPop(dirtyVec, &c)) {
        ChiObject* obj = chiObject(c);
        if (chiType(c) == CHI_STACK) {
            scanDirtyStack(state, c);
        } else if (!chiObjectShared(obj)) { // Check if object has been promoted to shared heap
            scanMajorObject(state, c);
            chiCountObject(state->stats.dirty.object, obj->size);
        }
    }
    chiObjVecFree(dirtyVec);
    CHI_TTP(state->proc, .name = "runtime;gc;scavenger;dirty objects");
}

static void scanDirtyCards(ScavengerState* state, ChiCardVec* oldVec) {
    ChiCardVec newVec;
    chiCardVecInit(&newVec, oldVec->manager);
    ChiCard card;
    while (chiCardVecPop(oldVec, &card)) {
        if (state->gc)
            chiMarkNormal(state->gc, card.obj);

        ChiObject* obj = chiObject(card.obj);
        bool* cardTable = (bool*)(obj->payload + obj->size);
        CHI_ASSERT(cardTable[card.idx]);

        bool dirty = false;
        for (Chili* p = (Chili*)obj->payload + CHI_CARD_SIZE * card.idx,
                 *end = CHI_MIN((Chili*)obj->payload + obj->size, p + CHI_CARD_SIZE); p < end; ++p)
            dirty |= copy(state, p, false);

        CHI_ASSERT(state->aging || !dirty);
        if (dirty)
            chiCardVecPush(&newVec, card);
        else
            cardTable[card.idx] = false;
    }
    chiCardVecFree(oldVec);
    chiCardVecFrom(oldVec, &newVec);
    CHI_TTP(state->proc, .name = "runtime;gc;scavenger;dirty cards");
}

static void moveOldBlocks(ScavengerState* state, ChiLocalHeap* h) {
    chiBlockListInit(&state->oldList);
    chiBlockListJoin(&state->oldList, &h->nursery.alloc.usedList);
    chiBlockListJoin(&state->oldList, &h->agedAlloc.usedList);
    h->nursery.alloc.block = h->agedAlloc.block = 0;
    chiBlockAllocFresh(&h->agedAlloc);
}

static void freeOldBlocks(ScavengerState* state, ChiLocalHeap* h) {
    h->nursery.bump = chiBlockAllocTake(&h->nursery.alloc);
    chiBlockManagerFreeList(&h->manager, &state->oldList);
    chiBlockAllocFresh(&h->nursery.alloc);
}

static void forwardPromoted(ChiPromotedVec* promoted) {
    CHI_BLOCKVEC_FOREACH(ChiPromotedVec, p, promoted) {
        CHI_ASSERT(chiGen(p->from) != CHI_GEN_MAJOR);
        CHI_ASSERT(chiGen(p->to) == CHI_GEN_MAJOR);
        *(Chili*)chiRawPayload(p->from) = p->to;
    }
    ChiBlockManager* bm = promoted->manager;
    chiPromotedVecFree(promoted);
    chiPromotedVecInit(promoted, bm);
}

void chiScavenger(ChiProcessor* proc, uint32_t aging, bool snapshot, ChiScavengerStats* stats) {
    ChiRuntime* rt = proc->rt;
    ChiLocalHeap* h = &proc->heap;
    const ChiRuntimeOption* opt = &rt->option;

    // Disable aging if dirty lists are too large
    if (h->dirty.cards.list.length + h->dirty.objects.list.length > h->nursery.limit / 4)
        aging = 0;

    ScavengerState state = {
        CHI_IF(CHI_COUNT_ENABLED, .stats = {
            .snapshot = snapshot,
            .aging = aging,
        },)
        .proc       = proc,
        .agedAlloc  = &h->agedAlloc,
        .blockSize  = opt->nursery.block,
        .blockMask  = CHI_ALIGNMASK(opt->nursery.block),
        .aging      = aging,
        .gc         = snapshot ? &proc->gc : 0,
        .black      = proc->gc.markState.black,
        .heapHandle = &proc->handle,
    };

    h->manager.allocHookEnabled = false;
    chiObjVecInit(&state.majorObjects, &h->manager);
    chiObjVecInit(&state.dirtyObjects, &h->manager);
    chiBlockAllocSetup(&state.cheneyAlloc, &h->manager);
    moveOldBlocks(&state, h);
    forwardPromoted(&h->promoted.objects);
    scanDirtyObjects(&state, &h->dirty.objects);
    scanDirtyCards(&state, &h->dirty.cards);
    scanCopiedObjects(&state);
    chiObjVecFrom(&h->dirty.objects, &state.dirtyObjects);
    CHI_ASSERT(chiObjVecNull(&h->dirty.objects) || state.aging);
    freeOldBlocks(&state, h);
    h->manager.allocHookEnabled = true;
    CHI_CHOICE(CHI_COUNT_ENABLED, *stats = state.stats, CHI_NOWARN_UNUSED(stats));
}
