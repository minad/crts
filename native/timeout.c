#include "timeout.h"

#if CHI_TIMEOUT_POLLING
void chiTimeoutStart(ChiTimeout* t, ChiMicros us) {
    t->_trigger = false;
    if (chiMicrosEq(us, CHI_TIMEOUT_INFINITE)) {
        t->_slice = 0;
        t->_ticks = INT32_MAX;
    } else {
        t->_time = chiClock(CHI_CLOCK_REAL_FINE);
        t->_slice = t->_ticks = 10;
        t->_timeout = chiNanosAdd(t->_time, chiMicrosToNanos(us));
    }
}

void chiTimeoutStop(ChiTimeout* t) {
    t->_trigger = true;
}

bool _chiTimeoutPoll(ChiTimeout* t) {
    if (t->_trigger)
        return false;
    if (t->_slice <= 0) {
        t->_ticks = INT32_MAX;
        return true;
    }
    ChiNanos time = chiClock(CHI_CLOCK_REAL_FINE);
    if (chiNanosLess(t->_timeout, time)) {
        t->_trigger = true;
        return false;
    }
    ChiNanos
        rest = chiNanosDelta(t->_timeout, time),
        delta = chiNanosDelta(time, t->_time);
    t->_time = time;
    int32_t ticks = (int32_t)CHI_CLAMP(t->_slice * chiNanosRatio(rest, delta), 2, 1e5) / 2;
    if (ticks <= 1) {
        t->_trigger = true;
        return false;
    }
    t->_ticks = t->_slice = ticks;
    return true;
}
#else
static ChiMicros timeoutHandler(ChiTimer* t) {
    chiTimeoutTrigger(CHI_OUTER(ChiTimeout, _timer, t), true);
    return (ChiMicros){0};
}

void chiTimeoutStart(ChiTimeout* t, ChiMicros us) {
    t->_trigger = false;
    if (chiMicrosEq(us, CHI_TIMEOUT_INFINITE)) {
        t->_timer = (ChiTimer){ .handler = 0 };
    } else {
        t->_timer = (ChiTimer){
            .handler = timeoutHandler,
            .timeout = us
        };
        chiTimerInstall(&t->_timer);
    }
}

void chiTimeoutStop(ChiTimeout* t) {
    t->_trigger = true;
    if (t->_timer.handler)
        chiTimerRemove(&t->_timer);
}
#endif
