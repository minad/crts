#include "sink.h"
#include "mark.h"
#include "chunk.h"
#include "runtime.h"
#include "stack.h"
#include "timeout.h"
#include <chili/object/thread.h>
#include <chili/object/thunk.h>

enum{
    MARK_RECURSIVE = 1,
    MARK_COLLAPSE  = 2,
};

/**
 * State of the object scanner
 * which runs over all gray objects and marks the
 * reachable objects as gray.
 */
typedef struct {
    ChiBlockVec  grayList;
    ChiScanStats stats[CHI_STATS_MARK_ENABLED];
    ChiTimeout*  timeout;
    uint32_t     depth;
    bool         noCollapse;
} ScanState;

CHI_INL void markCount(ChiObjectCount* count, size_t words) {
    if (CHI_STATS_MARK_ENABLED)
        chiCountObject(count, words);
}

CHI_INL void markCountCollapsed(ScanState* state) {
    if (CHI_STATS_MARK_ENABLED)
        ++state->stats->collapsed;
}

static void scan(ScanState*, Chili);

CHI_INL bool doMark(ScanState* state, Chili c, Chili* collapsed, uint32_t flags) {
    if (!chiReference(c) || !chiMajor(c))
        return false;

    ChiType type = chiType(c);
    ChiObject* o = chiObject(c);
    if (chiRaw(type)) {
        ChiColor color = chiObjectColor(o);
        CHI_ASSERT(color != CHI_GRAY);
        if (color != CHI_BLACK)
            chiObjectSetColor(o, CHI_BLACK);
        return false;
    }

    if (chiObjectCasColor(o, CHI_WHITE, CHI_GRAY)) {
        if ((flags & MARK_RECURSIVE) && state->depth > 0 && chiTimeoutTick(state->timeout)) {
            --state->depth;
            scan(state, c);
            ++state->depth;
        } else {
            chiBlockVecPush(&state->grayList, c);
        }
    }

    /*
     * Thunk collapsing is only available on 64 bit platforms for now,
     * since we overwrite the field with chiAtomicStore.
     * On 32 bit platforms, the non-atomic 64 bit reads will fail.
     *
     * Do not collapse unforced and nested thunks.
     *
     * Do not collapse if the thunk value is not part of the major heap,
     * such that we do not have to modify the dirty lists.
     */
    return CHI_ARCH_BITS == 64
        && (flags & MARK_COLLAPSE)
        && type == CHI_THUNK
        && !state->noCollapse
        && chiThunkCollapsible(c, collapsed)
        && (!chiReference(*collapsed) || chiMajor(*collapsed));
}

CHI_INL void markAtomic(ScanState* state, ChiAtomic* p, uint32_t flags) {
    for (;;) {
        Chili collapsed, c = chiAtomicLoad(p);
        if (!doMark(state, c, &collapsed, flags) || !chiAtomicCas(p, c, collapsed))
            break;
        markCountCollapsed(state);
    }
}

CHI_INL void mark(ScanState* state, Chili* p, uint32_t flags) {
    for (;;) {
        Chili collapsed;
        if (!doMark(state, *p, &collapsed, flags))
            break;
        *p = collapsed;
        markCountCollapsed(state);
    }
}

static void scanStack(ScanState* state, ChiObject* obj) {
    // Only scan the stack if it is inactive.
    // If it is active, the stack will be reinserted into the scan list
    // by the processor after deactivation.
    if (chiObjectCasActive(obj, false, true)) {
        ChiStack* stack = (ChiStack*)obj->payload;
        // mark non recursively since we don't want to keep the stack locked for long
        for (Chili* p = stack->base; p < stack->sp; ++p)
            mark(state, p, MARK_COLLAPSE);

        markCount(&state->stats->stack, (size_t)(stack->sp - stack->base));
        bool wasActive = chiObjectCasActive(obj, true, false);
        CHI_ASSERT(wasActive);
    }
}

static void scanThread(ScanState* state, ChiObject* obj) {
    ChiThread* t = (ChiThread*)obj->payload;
    markAtomic(state, &t->name, MARK_RECURSIVE);
    markAtomic(state, &t->state, MARK_RECURSIVE);
    markAtomic(state, &t->local, MARK_RECURSIVE);
    markAtomic(state, &t->stack, MARK_RECURSIVE);
    markCount(&state->stats->object, CHI_SIZEOF_WORDS(ChiThread));
}

static void scanThunk(ScanState* state, ChiObject* obj) {
    markAtomic(state, &((ChiThunk*)obj->payload)->val, MARK_RECURSIVE | MARK_COLLAPSE);
    markCount(&state->stats->object, CHI_SIZEOF_WORDS(ChiThunk));
}

static void scanStringBuilder(ScanState* state, ChiObject* obj) {
    markAtomic(state, &((ChiStringBuilder*)obj->payload)->string, MARK_RECURSIVE);
    markCount(&state->stats->object, CHI_SIZEOF_WORDS(ChiStringBuilder));
}

static void scanArray(ScanState* state, ChiObject* obj) {
    size_t size = chiObjectSize(obj);
    for (ChiAtomic* payload = (ChiAtomic*)obj->payload, *p = payload; p < payload + size; ++p)
        markAtomic(state, p, MARK_RECURSIVE | MARK_COLLAPSE);
    markCount(&state->stats->object, size);
}

static void scanImmutableObject(ScanState* state, ChiObject* obj) {
    size_t size = chiObjectSize(obj);
    for (Chili* payload = (Chili*)obj->payload, *p = payload; p < payload + size; ++p)
        mark(state, p, MARK_RECURSIVE | MARK_COLLAPSE);
    markCount(&state->stats->object, size);
}

static void scan(ScanState* state, Chili c) {
    CHI_ASSERT(chiReference(c));
    CHI_ASSERT(!chiRaw(chiType(c)));
    CHI_ASSERT(chiMajor(c));
    CHI_ASSERT(chiObjectColor(chiObject(c)) == CHI_GRAY);

    ChiObject* obj = chiObject(c);
    chiObjectSetColor(obj, CHI_BLACK);

    switch (chiType(c)) {
    case CHI_ARRAY: scanArray(state, obj); break;
    case CHI_THUNK: scanThunk(state, obj); break;
    case CHI_STRINGBUILDER: scanStringBuilder(state, obj); break;
    case CHI_THREAD: scanThread(state, obj); break;
    case CHI_STACK: scanStack(state, obj); break;
    case CHI_FIRST_TAG...CHI_LAST_IMMUTABLE: scanImmutableObject(state, obj); break;
    case CHI_RAW...CHI_LAST_TYPE: CHI_BUG("raw type");
    }
}

void chiScan(ChiBlockVec* grayList, ChiTimeout* timeout, ChiScanStats* stats, uint32_t depth, bool noCollapse) {
    ScanState state = { .timeout = timeout, .depth = depth, .noCollapse = noCollapse };
    chiBlockVecInit(&state.grayList, grayList->manager);
    chiBlockVecJoin(&state.grayList, grayList);
    Chili c;
    while (chiTimeoutTick(state.timeout) && chiBlockVecPop(&state.grayList, &c))
        scan(&state, c);
    chiBlockVecJoin(grayList, &state.grayList);
    *stats = CHI_STATS_MARK_ENABLED ? *state.stats : (ChiScanStats){};
}
