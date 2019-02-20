#define VEC TimerHeap
#define VEC_TYPE ChiTimer*
#define VEC_PREFIX timerHeap
#include "../vector.h"

static ChiMutex  interruptMutex;
static ChiList   sigHandler[_CHI_SIG_MAX+1];
static TimerHeap timerHeap = {};

static void timerSwap(TimerHeap* v, uint32_t i, uint32_t j) {
    CHI_SWAP(CHI_VEC_AT(v, i), CHI_VEC_AT(v, j));
    CHI_VEC_AT(v, i)->_index = i + 1;
    CHI_VEC_AT(v, j)->_index = j + 1;
}

static void timerCheck(TimerHeap* v, uint32_t i) {
    if (CHI_DEFINED(NDEBUG))
        return;
    if (i >= v->used)
        return;
    if (CHI_VEC_AT(v, i)->_index != i + 1)
        chiAbort("Invalid heap index");
    uint32_t l = 2 * i + 1, r = 2 * i + 2;
    if (l < v->used && chiMicrosLess(CHI_VEC_AT(v, l)->_timeoutAbs, CHI_VEC_AT(v, i)->_timeoutAbs))
        chiAbort("Heap condition violated for left %u", l);
    if (r < v->used && chiMicrosLess(CHI_VEC_AT(v, r)->_timeoutAbs, CHI_VEC_AT(v, i)->_timeoutAbs))
        chiAbort("Heap condition violated for right %u", r);
    timerCheck(v, l);
    timerCheck(v, r);
}

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
    t->_timeoutAbs = (ChiMicros){0};
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
    timerHeapPush(v, t);
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
            return (ChiMicros){0};
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
    CHI_LOCK(&interruptMutex);
    chiListPoison(&handler->_list);
    chiListAppend(sigHandler + handler->sig, &handler->_list);
}

void chiSigRemove(ChiSigHandler* handler) {
    CHI_LOCK(&interruptMutex);
    chiListDelete(&handler->_list);
}

static void dispatcherNotify(void);

void chiTimerInstall(ChiTimer* t) {
    CHI_ASSERT(chiMicrosZero(t->_timeoutAbs));
    CHI_ASSERT(t->handler);
    t->_timeoutAbs = chiMicrosAdd(t->timeout, chiNanosToMicros(chiClock(CHI_CLOCK_REAL_FINE)));
    bool updated;
    {
        CHI_LOCK(&interruptMutex);
        timerInsert(&timerHeap, t);
        updated = t->_index == 1;
    }
    if (updated)
        dispatcherNotify();
}

void chiTimerRemove(ChiTimer* t) {
    CHI_LOCK(&interruptMutex);
    timerRemove(&timerHeap, t);
    t->handler = 0;
}

static void interruptSetup(void) {
    chiMutexInit(&interruptMutex);
    for (size_t i = 0; i < CHI_DIM(sigHandler); ++i)
        chiListInit(sigHandler + i);
}

static ChiMicros dispatchTimers(void) {
    ChiMicros us = chiNanosToMicros(chiClock(CHI_CLOCK_REAL_FINE));
    CHI_LOCK(&interruptMutex);
    return timerDispatch(&timerHeap, us);
}

static void dispatchSig(ChiSig sig) {
    CHI_LOCK(&interruptMutex);
    CHI_LIST_FOREACH_NODELETE(ChiSigHandler, _list, handler, sigHandler + sig)
        handler->handler(handler);
}
