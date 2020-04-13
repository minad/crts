#include "barrier.h"
#include "sink.h"
#include "stack.h"
#include "timeout.h"

CHI_FLAGTYPE(MarkFlags,
             MARK_RECURSIVE = 1,
             MARK_COLLAPSE  = 2)

/**
 * State of the object scanner
 * which runs over all gray objects and marks the
 * reachable objects as gray.
 */
typedef struct {
    ChiBlockVec   gray;
    ChiTimeout*   timeout;
    uint32_t      depth;
    bool          collapse;
    ChiMarkState markState;
    CHI_IF(CHI_COUNT_ENABLED, ChiScanStats stats;)
} MarkState;

static void scan(MarkState*, Chili);

CHI_INL bool _mark(MarkState* state, Chili c, Chili* collapsed, MarkFlags flags) {
    if (chiUnboxed(c) || chiGen(c) != CHI_GEN_MAJOR)
        return false;

    CHI_CHECK_OBJECT_ALIVE(state->markState, c);
    ChiType type = chiType(c);
    ChiObject* obj = chiObjectUnchecked(c);

    if (chiRaw(type)) {
        chiObjectSetColor(obj, state->markState.black);
        return false;
    }

    ChiColor color = chiObjectColor(obj);
    if (chiColorEq(color, state->markState.white)) {
        chiObjectSetColor(obj, state->markState.gray);
        if ((flags & MARK_RECURSIVE) && state->depth > 0 && chiTimeoutTick(state->timeout)) {
            --state->depth;
            scan(state, c);
            ++state->depth;
        } else {
            chiBlockVecPush(&state->gray, c);
        }
    }

    /*
     * Thunk collapsing is only available on 64-bit platforms for now,
     * since we overwrite the 64-bit field non-atomically while mutators
     * might be accessing it. On 32-bit platforms, the non-atomic 64-bit reads
     * might read a partial value.
     * TODO: One possible solution could be to implement thunk collapsing as
     * during a short stop the world pause. The collapsible thunks should then
     * be collected in a queue.
     *
     * Do not collapse unforced and nested thunks.
     */
    if (!CHI_ARCH_32BIT
        && (flags & MARK_COLLAPSE)
        && type == CHI_THUNK
        && state->collapse) {
        *collapsed = chiFieldRead(&((ChiThunk*)obj->payload)->val);
        return chiThunkCollapsible(*collapsed);
    }

    return false;
}

CHI_INL void markAtomic(MarkState* state, ChiField* p, MarkFlags flags) {
    for (;;) {
        Chili collapsed, c = chiFieldRead(p);
        if (!_mark(state, c, &collapsed, flags) || !chiAtomicCas(p, c, collapsed))
            break;
        chiCount(state->stats.collapsed, 1);
    }
}

CHI_INL void mark(MarkState* state, Chili* p, MarkFlags flags) {
    for (;;) {
        Chili collapsed;
        if (!_mark(state, *p, &collapsed, flags))
            break;
        *p = collapsed;
        chiCount(state->stats.collapsed, 1);
    }
}

static void scanStack(MarkState* state, Chili c) {
    CHI_CHECK_OBJECT_ALIVE(state->markState, c);
    ChiObject* obj = chiObjectUnchecked(c);
    /* If we fail to lock the stack, the stack scanning
     * is taken care of by the mutator in chiStackActivate
     * or by the scavenger.
     */
    if (chiObjectTryLock(obj)) {
        ChiColor color = chiObjectColor(obj);
        if (!chiColorEq(color, state->markState.black)) {
            chiObjectSetColor(obj, state->markState.black);
            ChiStack* stack = (ChiStack*)obj->payload;
            // mark non recursively since we don't want to keep the stack locked for long
            for (Chili* p = stack->base; p < stack->sp; ++p)
                mark(state, p, MARK_COLLAPSE);
            chiCountObject(state->stats.stack, (size_t)(stack->sp - stack->base));
        }
        chiObjectUnlock(obj);
    }
}

static void scanThread(MarkState* state, ChiObject* obj) {
    ChiThread* t = (ChiThread*)obj->payload;
    markAtomic(state, &t->state, MARK_RECURSIVE);
    scanStack(state, chiFieldRead(&t->stack));
    chiCountObject(state->stats.object, CHI_SIZEOF_WORDS(ChiThread));
}

static void scanThunk(MarkState* state, ChiObject* obj) {
    markAtomic(state, &((ChiThunk*)obj->payload)->val, MARK_RECURSIVE | MARK_COLLAPSE);
    chiCountObject(state->stats.object, CHI_SIZEOF_WORDS(ChiThunk));
}

static void scanStringBuilder(MarkState* state, ChiObject* obj) {
    markAtomic(state, &((ChiStringBuilder*)obj->payload)->buf, MARK_RECURSIVE);
    chiCountObject(state->stats.object, CHI_SIZEOF_WORDS(ChiStringBuilder));
}

static void scanArray(MarkState* state, ChiObject* obj) {
    size_t size = chiObjectSize(obj);
    for (ChiField* payload = (ChiField*)obj->payload, *p = payload; p < payload + size; ++p)
        markAtomic(state, p, MARK_RECURSIVE | MARK_COLLAPSE);
    chiCountObject(state->stats.object, size);
}

static void scanImmutableObject(MarkState* state, ChiObject* obj) {
    size_t size = chiObjectSize(obj);
    for (Chili* payload = (Chili*)obj->payload, *p = payload; p < payload + size; ++p)
        mark(state, p, MARK_RECURSIVE | MARK_COLLAPSE);
    chiCountObject(state->stats.object, size);
}

static void scan(MarkState* state, Chili c) {
    CHI_ASSERT(!chiUnboxed(c));
    CHI_ASSERT(!chiRaw(chiType(c)));
    CHI_ASSERT(chiGen(c) == CHI_GEN_MAJOR);

    CHI_CHECK_OBJECT_ALIVE(state->markState, c);
    ChiObject* obj = chiObjectUnchecked(c);
    ChiColor color = chiObjectColor(obj);
    if (!chiColorEq(color, state->markState.gray))
        return;
    chiObjectSetColor(obj, state->markState.black);

    switch (chiType(c)) {
    case CHI_ARRAY: scanArray(state, obj); break;
    case CHI_THUNK: scanThunk(state, obj); break;
    case CHI_STRINGBUILDER: scanStringBuilder(state, obj); break;
    case CHI_THREAD: scanThread(state, obj); break;
    case CHI_FIRST_TAG...CHI_LAST_IMMUTABLE: scanImmutableObject(state, obj); break;
    case CHI_STACK...CHI_LAST_TYPE: CHI_BUG("Invalid value %C found in gray list", c);
    }
}

void chiMarkSlice(ChiGrayVec* gray, uint32_t depth, bool collapse, ChiMarkState markState,
                  ChiScanStats* stats, ChiTimeout* timeout) {
    MarkState state =
        { .timeout = timeout,
          .depth = depth,
          .collapse = collapse,
          .markState = markState,
        };
    chiBlockVecInit(&state.gray, gray->vec.manager);
    chiBlockVecJoin(&state.gray, &gray->vec);
    Chili c;
    while (chiTimeoutTick(timeout) && chiBlockVecPop(&state.gray, &c))
        scan(&state, c);
    chiBlockVecJoin(&gray->vec, &state.gray);
    chiGrayUpdateNull(gray);
    CHI_CHOICE(CHI_COUNT_ENABLED, *stats = state.stats, CHI_NOWARN_UNUSED(stats));
}
