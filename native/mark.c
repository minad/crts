#include "barrier.h"
#include "sink.h"
#include "stack.h"
#include "timeout.h"

CHI_FLAGTYPE(MarkFlags,
             MARK_DEFAULT  = 0,
             MARK_RECURSE  = 1,
             MARK_COLLAPSE = 2)

/**
 * State of the object scanner
 * which runs over all gray objects and marks the
 * reachable objects as gray.
 */
typedef struct {
    ChiObjVec    gray;
    ChiTimeout*  timeout;
    uint32_t     depth;
    ChiMarkState markState;
    CHI_IF(CHI_COUNT_ENABLED, ChiScanStats stats;)
} MarkState;

static void scan(MarkState*, Chili);

CHI_INL bool doMark(MarkState* state, Chili c, Chili* collapsed, MarkFlags flags) {
    if (!chiRefMajor(c))
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
        if ((flags & MARK_RECURSE) && state->depth > 0 && chiTimeoutTick(state->timeout)) {
            --state->depth;
            scan(state, c);
            ++state->depth;
        } else {
            chiObjVecPush(&state->gray, c);
        }
    }

    // Do not collapse unforced and nested thunks.
    if (CHI_MARK_COLLAPSE_ENABLED
        && (flags & MARK_COLLAPSE)
        && type == CHI_THUNK)
        return chiThunkCollapsible((ChiThunk*)obj->payload, collapsed);

    return false;
}

CHI_INL void markAtomic(MarkState* state, ChiField* p, MarkFlags flags) {
    for (;;) {
        Chili collapsed, c = chiFieldRead(p);
        if (!doMark(state, c, &collapsed, flags) || !chiAtomicCas(p, c, collapsed))
            break;
        chiCount(state->stats.collapsed, 1);
    }
}

CHI_INL void mark(MarkState* state, Chili* p, MarkFlags flags) {
    for (;;) {
        Chili collapsed;
        if (!doMark(state, *p, &collapsed, flags))
            break;
        *p = collapsed;
        chiCount(state->stats.collapsed, 1);
    }
}

static void scanStack(MarkState* state, ChiObject* obj) {
    /* If we fail to lock the stack, the stack scanning
     * is taken care of by the mutator in chiStackActivate
     * or by the scavenger.
     */
    if (chiObjectTryLock(obj)) {
        ChiColor color = chiObjectColor(obj);
        if (!chiColorEq(color, state->markState.black)) {
            chiObjectSetColor(obj, state->markState.black);
            ChiStack* stack = (ChiStack*)obj->payload;
            /* No MARK_RECUSIVE: We don't want to keep the stack locked for long
             * No MARK_COLLAPSE: updateCont fails if the thunks on the stack are
             *                   collapsed behind its back.
             */
            for (Chili* p = stack->base; p < stack->sp; ++p)
                mark(state, p, MARK_DEFAULT);
            chiCountObject(state->stats.stack, (size_t)(stack->sp - stack->base));
        }
        chiObjectUnlock(obj);
    }
}

static bool alreadyBlack(MarkState* state, ChiObject* obj) {
    ChiColor color = chiObjectColor(obj);
    if (chiColorEq(color, state->markState.black))
        return true;
    chiObjectSetColor(obj, state->markState.black);
    return false;
}

static void scanThread(MarkState* state, ChiObject* obj) {
    if (alreadyBlack(state, obj))
        return;
    ChiThread* t = (ChiThread*)obj->payload;
    markAtomic(state, &t->state, MARK_RECURSE);
    markAtomic(state, &t->stack, MARK_RECURSE);
    chiCountObject(state->stats.object, CHI_SIZEOF_WORDS(ChiThread));
}

static void scanThunk(MarkState* state, ChiObject* obj) {
    if (alreadyBlack(state, obj))
        return;
    chiCountObject(state->stats.object, obj->size);
    ChiThunk* thunk = (ChiThunk*)obj->payload;
    markAtomic(state, &thunk->val, MARK_RECURSE | MARK_COLLAPSE);
    for (Chili *p = thunk->clos, *end = p + obj->size - 2; p < end; ++p)
        mark(state, p, MARK_RECURSE | MARK_COLLAPSE);
}

static void scanStringBuilder(MarkState* state, ChiObject* obj) {
    if (alreadyBlack(state, obj))
        return;
    chiCountObject(state->stats.object, CHI_SIZEOF_WORDS(ChiStringBuilder));
    markAtomic(state, &((ChiStringBuilder*)obj->payload)->buf, MARK_RECURSE);
}

static void scanArray(MarkState* state, ChiObject* obj) {
    if (alreadyBlack(state, obj))
        return;
    chiCountObject(state->stats.object, obj->size);
    for (ChiField *p = (ChiField*)obj->payload, *end = p + obj->size; p < end; ++p)
        markAtomic(state, p, MARK_RECURSE | MARK_COLLAPSE);
}

static void scanImmutableObject(MarkState* state, ChiObject* obj) {
    if (alreadyBlack(state, obj))
        return;
    chiCountObject(state->stats.object, obj->size);
    for (Chili *p = (Chili*)obj->payload, *end = p + obj->size; p < end; ++p)
        mark(state, p, MARK_RECURSE | MARK_COLLAPSE);
}

static void scan(MarkState* state, Chili c) {
    CHI_ASSERT(chiRefMajor(c));
    CHI_ASSERT(!chiRaw(chiType(c)));
    CHI_CHECK_OBJECT_ALIVE(state->markState, c);
    ChiObject* obj = chiObjectUnchecked(c);
    switch (chiType(c)) {
    case CHI_ARRAY_SMALL: case CHI_ARRAY_LARGE: scanArray(state, obj); break;
    case CHI_THUNK: scanThunk(state, obj); break;
    case CHI_STRINGBUILDER: scanStringBuilder(state, obj); break;
    case CHI_THREAD: scanThread(state, obj); break;
    case CHI_STACK: scanStack(state, obj); break;
    case CHI_FIRST_TAG...CHI_LAST_IMMUTABLE: scanImmutableObject(state, obj); break;
    case CHI_POISON_TAG...CHI_LAST_TYPE: CHI_BUG("Invalid value %C found in gray list", c);
    }
}

void chiMarkSlice(ChiGrayVec* gray, uint32_t depth, ChiMarkState markState,
                  ChiScanStats* stats, ChiTimeout* timeout) {
    MarkState state =
        { .timeout = timeout,
          .depth = depth,
          .markState = markState,
        };
    chiObjVecFrom(&state.gray, &gray->vec);
    Chili c;
    while (chiTimeoutTick(timeout) && chiObjVecPop(&state.gray, &c))
        scan(&state, c);
    chiObjVecFrom(&gray->vec, &state.gray);
    chiGrayVecUpdateNull(gray);
    CHI_CHOICE(CHI_COUNT_ENABLED, *stats = state.stats, CHI_NOWARN_UNUSED(stats));
}
