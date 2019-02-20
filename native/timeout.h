#pragma once

#include "system.h"

typedef struct ChiTimeout_ ChiTimeout;

#define CHI_TIMEOUT_INFINITE ((ChiMicros){~0ULL})

void chiTimeoutStart(ChiTimeout*, ChiMicros);
void chiTimeoutStop(ChiTimeout*);

#if CHI_TIMEOUT_POLLING
struct ChiTimeout_ {
    ChiNanos _timeout, _time;
    int32_t _slice, _ticks;
    volatile bool _trigger;
};

bool _chiTimeoutPoll(ChiTimeout*);

CHI_INL bool chiTimeoutTick(ChiTimeout* t) {
    return CHI_LIKELY(--t->_ticks > 0) || _chiTimeoutPoll(t);
}
#else
_Static_assert(CHI_SYSTEM_HAS_INTERRUPT, "System does not support timer interrupts.");

struct ChiTimeout_ {
    ChiTimer _timer;
    volatile bool _trigger;
};

CHI_INL bool chiTimeoutTick(ChiTimeout* t) {
    return CHI_LIKELY(!t->_trigger);
}
#endif

CHI_INL void chiTimeoutTrigger(ChiTimeout* t, bool set) {
    t->_trigger = set;
}
