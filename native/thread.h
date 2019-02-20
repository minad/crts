#pragma once

#include "private.h"
#include <chili/object/thread.h>

/**
 * Thread states. Must be sorted alphabetically.
 */
enum {
    CHI_TS_RUNNING,
    CHI_TS_TERMINATED,
    CHI_TS_WAIT_BLACKHOLE,
    CHI_TS_WAIT_SLEEP,
    CHI_TS_WAIT_THREAD,
};

typedef struct ChiProcessor_ ChiProcessor;

CHI_WU Chili chiThreadNewUninitialized(ChiProcessor*);
void chiThreadScheduler(ChiProcessor*);
void chiThreadSetNameField(ChiProcessor*, Chili, Chili);
void chiThreadSetStateField(ChiProcessor*, Chili, Chili);
CHI_WU Chili chiThreadStack(Chili);

/// Use chiThreadStack instead, which checks that the stack is activated
CHI_INL CHI_WU Chili chiThreadStackUnchecked(Chili c) {
    return chiAtomicLoad(&chiToThread(c)->stack);
}

#define CHI_THREAD_EVENT(begin, proc)                               \
    ({                                                              \
        if (chiIdentical((proc)->thread, (proc)->schedulerThread))  \
            CHI_EVENT0((proc), THREAD_SCHED_##begin);               \
        else                                                        \
            CHI_EVENT0((proc), THREAD_RUN_##begin);                 \
    })
