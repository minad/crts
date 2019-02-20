#include "thread.h"
#include "stack.h"
#include "runtime.h"
#include "event.h"
#include "barrier.h"

Chili chiThreadNewUninitialized(ChiProcessor* proc) {
    Chili c = chiNew(CHI_THREAD, CHI_SIZEOF_WORDS (ChiThread));
    ChiThread* t = chiToThread(c);
    ++proc->rt->threadCount;
    uint32_t tid = proc->rt->nextTid++ & UINT32_MAX;
    t->tid = chiFromUnboxed(tid);
    t->exceptBlock = t->lastErrno = CHI_FALSE;
    chiAtomicInit(&t->stack, chiStackNew(proc));
    chiAtomicInit(&t->name, CHI_FALSE);
    chiAtomicInit(&t->local, CHI_FALSE);
    chiAtomicInit(&t->state, chiNewEmpty(CHI_TS_RUNNING));
    return c;
}

void chiThreadSetStateField(ChiProcessor* proc, Chili thread, Chili state) {
    chiWriteField(proc, thread, &chiToThread(thread)->state, state);
}

void chiThreadSetNameField(ChiProcessor* proc, Chili thread, Chili name) {
    ChiThread* t = chiToThread(thread);
    chiWriteField(proc, thread, &t->name, name);
    CHI_EVENT(proc, THREAD_NAME,
              .tid = (uint32_t)chiToUnboxed(t->tid),
              .name = chiStringRef(&name));
}

void chiThreadSetState(Chili thread, Chili state) {
    chiThreadSetStateField(CHI_CURRENT_PROCESSOR, thread, state);
}

void chiThreadSetName(Chili thread, Chili name) {
    chiThreadSetNameField(CHI_CURRENT_PROCESSOR, thread, name);
}

void chiThreadSetLocal(Chili thread, Chili local) {
    chiWriteField(CHI_CURRENT_PROCESSOR, thread, &chiToThread(thread)->local, local);
}

Chili chiThreadCurrent(void) {
    return CHI_CURRENT_PROCESSOR->thread;
}

Chili chiThreadNew(Chili run) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    Chili c = chiThreadNewUninitialized(proc);
    ChiStack* s = chiToStack(chiThreadStackUnchecked(c));
    s->sp[0] = chiFromCont(&chiApp1);
    s->sp[1] = run;
    s->sp[2] = CHI_FALSE;
    s->sp[3] = chiFromUnboxed(5);
    s->sp[4] = chiFromCont(&chiEnterCont);
    s->sp += 5;
    CHI_EVENT(proc, THREAD_NEW,
              .tid = (uint32_t)chiToUnboxed(chiToThread(c)->tid),
              .count = proc->rt->threadCount);
    return c;
}

void chiThreadScheduler(ChiProcessor* proc) {
    CHI_ASSERT(chiTrue(proc->rt->entryPoint.scheduler));
    CHI_ASSERT(!chiIdentical(proc->thread, proc->schedulerThread));

    if (chiTag(chiThreadState(proc->thread)) == CHI_TS_TERMINATED) {
        CHI_ASSERT(proc->rt->threadCount > 0);
        --proc->rt->threadCount;
        CHI_EVENT(proc, THREAD_TERMINATED, .count = proc->rt->threadCount);
    }

    ChiStack* s = chiToStack(chiThreadStackUnchecked(proc->schedulerThread));
    CHI_ASSERT(s->sp == s->base);
    s->sp[0] = chiFromCont(&chiApp2);
    s->sp[1] = proc->rt->entryPoint.scheduler;
    s->sp[2] = proc->thread;
    s->sp[3] = proc->rt->switchThreadClos;
    s->sp[4] = chiFromUnboxed(6);
    s->sp[5] = chiFromCont(&chiEnterCont);
    s->sp += 6;

    proc->thread = proc->schedulerThread;

    CHI_ASSERT(chiTag(chiThreadState(proc->thread)) == CHI_TS_RUNNING);
}

Chili chiThreadStack(Chili c) {
    Chili stack = chiThreadStackUnchecked(c);
    CHI_ASSERT(chiObjectActive(chiObject(stack)));
    return stack;
}
