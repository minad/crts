#pragma once

#include "system.h"

typedef struct ChiTimeout_ ChiTimeout;

#define CHI_TIMEOUT_INFINITE chiMicros((uint64_t)-1)

CHI_INTERN void chiTimeoutStart(ChiTimeout*, ChiMicros);
CHI_INTERN void chiTimeoutStop(ChiTimeout*);

#if CHI_TIMEOUT_INTERRUPT_ENABLED
_Static_assert(CHI_SYSTEM_HAS_INTERRUPT, "System does not support timer interrupts.");

struct ChiTimeout_ {
    ChiTimer _timer;
    ChiTrigger _trigger;
};

CHI_INL CHI_WU bool chiTimeoutTick(ChiTimeout* t) {
    return CHI_LIKELY(!chiTriggered(&t->_trigger));
}
#else
struct ChiTimeout_ {
    ChiNanos _timeout, _time;
    int32_t _slice, _ticks;
    ChiTrigger _trigger;
};

CHI_INTERN bool chiTimeoutPoll(ChiTimeout*);

CHI_INL CHI_WU bool chiTimeoutTick(ChiTimeout* t) {
    return CHI_LIKELY(--t->_ticks > 0) || chiTimeoutPoll(t);
}
#endif

CHI_INL void chiTimeoutTrigger(ChiTimeout* t, bool set) {
    chiTrigger(&t->_trigger, set);
}
