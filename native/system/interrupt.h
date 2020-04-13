#define VEC TimerHeap
#define VEC_TYPE ChiTimer*
#define VEC_PREFIX timerHeap
#include "../generic/vec.h"

#define LIST            SigHandlerList
#define LIST_PREFIX     sigHandlerList
#define LIST_ELEM       ChiSigHandler
#define LIST_LINK_FIELD _link
#include "../generic/list.h"

static ChiMutex interruptMutex = CHI_MUTEX_STATIC_INIT;
static SigHandlerList sigHandler = CHI_LIST_INIT(sigHandler);
static TimerHeap timerHeap = {};

static void timerSwap(TimerHeap* v, uint32_t i, uint32_t j) {
    CHI_SWAP(CHI_VEC_AT(v, i), CHI_VEC_AT(v, j));
    CHI_VEC_AT(v, i)->_index = i + 1;
    CHI_VEC_AT(v, j)->_index = j + 1;
}

#ifdef NDEBUG
static void timerCheck(TimerHeap* CHI_UNUSED(v), uint32_t CHI_UNUSED(i)) {}
#else
static void timerCheck(TimerHeap* v, uint32_t i) {
    if (i >= v->used)
        return;
    if (CHI_VEC_AT(v, i)->_index != i + 1)
        chiErr("Invalid heap index");
    uint32_t l = 2 * i + 1, r = 2 * i + 2;
    if (l < v->used && chiMicrosLess(CHI_VEC_AT(v, l)->_timeoutAbs, CHI_VEC_AT(v, i)->_timeoutAbs))
        chiErr("Heap condition violated for left %u", l);
    if (r < v->used && chiMicrosLess(CHI_VEC_AT(v, r)->_timeoutAbs, CHI_VEC_AT(v, i)->_timeoutAbs))
        chiErr("Heap condition violated for right %u", r);
    timerCheck(v, l);
    timerCheck(v, r);
}
#endif

static void timerHeapify(TimerHeap* v, uint32_t i) {
    for (;;) {
        uint32_t l = 2 * i + 1, r = 2 * i + 2, min = i;
        if (l < v->used && chiMicrosLess(CHI_VEC_AT(v, l)->_timeoutAbs, CHI_VEC_AT(v, min)->_timeoutAbs))
            min = l;
        if (r < v->used && chiMicrosLess(CHI_VEC_AT(v, r)->_timeoutAbs, CHI_VEC_AT(v, min)->_timeoutAbs))
            min = r;
        if (min == i)
            break;
        timerSwap(v, min, i);
        i = min;
    }
    timerCheck(v, 0);
}

static void timerRemove(TimerHeap* v, ChiTimer* t) {
    if (!t->_index)
        return;
    uint32_t i = t->_index - 1;
    CHI_ASSERT(v->used > 0);
    CHI_ASSERT(!chiMicrosZero(t->_timeoutAbs));
    t->_index = 0;
    t->_timeoutAbs = chiMicros(0);
    if (!v->used)
        return;
    CHI_VEC_AT(v, i) = CHI_VEC_AT(v, v->used - 1);
    CHI_VEC_AT(v, i)->_index = i + 1;
    --v->used;
    timerHeapify(v, i);
}

static void timerInsert(TimerHeap* v, ChiTimer* t) {
    CHI_NOWARN_UNUSED(timerHeapFree);
    CHI_ASSERT(!chiMicrosZero(t->_timeoutAbs));
    timerHeapAppend(v, t);
    uint32_t i = (uint32_t)(v->used - 1), p = (i - 1) / 2;
    t->_index = i + 1;
    while (i && chiMicrosLess(CHI_VEC_AT(v, i)->_timeoutAbs, CHI_VEC_AT(v, p)->_timeoutAbs)) {
        timerSwap(v, p, i);
        i = p;
        p = (i - 1) / 2;
    }
    timerCheck(v, 0);
}

static ChiTimer* timerTop(const TimerHeap* v) {
    return v->used ? CHI_VEC_AT(v, 0) : 0;
}

static ChiMicros timerDispatch(TimerHeap* v, ChiMicros us) {
    for (;;) {
        ChiTimer* t = timerTop(v);
        if (!t)
            return chiMicros(0);
        if (chiMicrosLess(us, t->_timeoutAbs))
            return chiMicrosDelta(t->_timeoutAbs, us);
        timerRemove(v, t);
        ChiMicros next = t->handler(t);
        if (!chiMicrosZero(next)) {
            t->_timeoutAbs = chiMicrosAdd(next, us);
            timerInsert(v, t);
        }
    }
}

void chiSigInstall(ChiSigHandler* handler) {
    CHI_LOCK_MUTEX(&interruptMutex);
    sigHandlerListPoison(handler);
    sigHandlerListAppend(&sigHandler, handler);
}

void chiSigRemove(ChiSigHandler* handler) {
    CHI_LOCK_MUTEX(&interruptMutex);
    sigHandlerListDelete(handler);
}

static void dispatcherNotify(void);

void chiTimerInstall(ChiTimer* t) {
    CHI_ASSERT(chiMicrosZero(t->_timeoutAbs));
    CHI_ASSERT(t->handler);
    t->_timeoutAbs = chiMicrosAdd(t->timeout, chiNanosToMicros(chiClockMonotonicFine()));
    bool updated;
    {
        CHI_LOCK_MUTEX(&interruptMutex);
        timerInsert(&timerHeap, t);
        updated = t->_index == 1;
    }
    if (updated)
        dispatcherNotify();
}

void chiTimerRemove(ChiTimer* t) {
    CHI_LOCK_MUTEX(&interruptMutex);
    timerRemove(&timerHeap, t);
    t->handler = 0;
}

static ChiMicros dispatchTimers(void) {
    ChiMicros us = chiNanosToMicros(chiClockMonotonicFine());
    CHI_LOCK_MUTEX(&interruptMutex);
    return timerDispatch(&timerHeap, us);
}

static void dispatchSig(ChiSig sig) {
    CHI_LOCK_MUTEX(&interruptMutex);
    CHI_LIST_FOREACH_NODELETE(ChiSigHandler, _link, handler, &sigHandler)
        handler->handler(handler, sig);
}
