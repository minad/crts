#include "barrier.h"
#include "event.h"
#include "new.h"
#include "stack.h"

typedef enum {
    TS_ERRNO = 1,
    TS_INTERRUPTIBLE = 3,
    TS_NAME = 5,
    TS_TID = 8,
} ThreadState;

static void threadSet(Chili thread, ThreadState ts, Chili val) {
    CHI_ASSERT(chiUnboxed(val));
    Chili c = chiThreadState(thread);
    // No chiFieldWrite barrier needed, since only unboxed values are written!
    if (chiTrue(c))
        chiAtomicWrite(chiArrayField(chiIdx(c, ts), 0), val);
}

static Chili threadGet(Chili thread, ThreadState ts, Chili dfl) {
    Chili c = chiThreadState(thread);
    return chiTrue(c) ? chiArrayRead(chiIdx(c, ts), 0) : dfl;
}

Chili chiThreadNewUninitialized(ChiProcessor* proc) {
    Chili c = chiNewInl(proc, CHI_THREAD, CHI_SIZEOF_WORDS (ChiThread), CHI_NEW_SHARED | CHI_NEW_CLEAN);
    ChiThread* t = chiToThread(c);
    chiFieldInit(&t->stack, chiStackNew(proc));
    chiFieldInit(&t->state, CHI_FALSE);
    return c;
}

Chili chiThreadName(Chili thread) {
    return threadGet(thread, TS_NAME, CHI_FALSE);
}

bool chiThreadInterruptible(Chili thread) {
    return chiToBool(threadGet(thread, TS_INTERRUPTIBLE, CHI_FALSE));
}

void chiThreadSetErrno(ChiProcessor* proc, int32_t e) {
    threadSet(proc->thread, TS_ERRNO, chiFromInt32(e));
}

void chiThreadSetInterruptible(ChiProcessor* proc, bool b) {
    threadSet(proc->thread, TS_INTERRUPTIBLE, chiFromBool(b));
}

uint32_t chiThreadId(Chili thread) {
    Chili c = chiThreadState(thread);
    return chiTrue(c) ? chiToUInt32(chiIdx(c, TS_TID)) : 0;
}

void chiThreadInitState(Chili c) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiField* field = &chiToThread(proc->thread)->state;
    CHI_ASSERT(!chiTrue(chiFieldRead(field)));
    chiPromoteShared(proc, &c);
    chiAtomicWrite(field, c);
}

Chili chiThreadNew(Chili state, Chili run) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;

    // Promote closure to shared space. This isolates the thread!
    ChiPromoteState ps;
    chiPromoteSharedBegin(&ps, proc);
    chiPromoteSharedObject(&ps, &state);
    chiPromoteSharedObject(&ps, &run);
    chiPromoteSharedEnd(&ps);

    Chili c = chiThreadNewUninitialized(proc);
    chiFieldInit(&chiToThread(c)->state, state);
    Chili stack = chiThreadStackInactive(c);
    ChiStack* s = chiToStack(stack);
    s->sp[0] = chiFromCont(&chiApp1);
    s->sp[1] = run;
    s->sp[2] = CHI_FALSE;
    s->sp[3] = chiFromUnboxed(5);
    s->sp[4] = chiFromCont(&chiRestoreCont);
    s->sp += 5;
    chiEvent(proc, THREAD_NEW, .newTid = chiThreadId(c), .newStack = chiAddress(stack));
    return c;
}
