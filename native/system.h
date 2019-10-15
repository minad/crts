#pragma once

#include <stdlib.h>
#include "event/time.h"

typedef struct ChiTimer_ ChiTimer;
typedef struct ChiSigHandler_ ChiSigHandler;

enum {
    CHI_EXIT_SUCCESS = 0,
    CHI_EXIT_ERROR   = 1,
    CHI_EXIT_FATAL   = 2,
};

typedef enum {
    CHI_CLOCK_CPU,
    CHI_CLOCK_REAL_FINE,
    CHI_CLOCK_REAL_FAST,
} ChiClock;

typedef struct {
    ChiNanos real, cpu;
} ChiTime;

struct ChiTimer_ {
    ChiMicros (*handler)(ChiTimer*);
    ChiMicros timeout;
    ChiMicros _timeoutAbs;
    uint32_t  _index;
};

enum {
    CHI_FILE_READABLE   = 1,
    CHI_FILE_EXECUTABLE = 2,
};

#if CHI_ENV_ENABLED
CHI_INTERN char* chiGetEnv(const char*);
#else
CHI_INL char* chiGetEnv(const char* CHI_UNUSED(v)) { return 0; }
#endif

#if defined(CHI_STANDALONE_SANDBOX)
#  include "system/sandbox.h"
#elif defined(CHI_STANDALONE_WASM)
#  include "system/wasm.h"
#elif defined(_WIN32)
#  include "system/win.h"
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__MACH__)
#  include "system/posix.h"
#else
#  error Unsupported operation system
#endif

typedef struct ChiSigHandlerLink_ ChiSigHandlerLink;
struct ChiSigHandlerLink_ { ChiSigHandlerLink *prev, *next; };
struct ChiSigHandler_ {
    void (*handler)(ChiSigHandler*, ChiSig);
    ChiSigHandlerLink _link;
};

CHI_DEFINE_AUTO_LOCK(ChiMutex, chiMutexLock, chiMutexUnlock)
#define CHI_LOCK_MUTEX(m) CHI_AUTO_LOCK(ChiMutex, m)

CHI_INL ChiTime chiTime(void) {
    return (ChiTime){ .real = chiClock(CHI_CLOCK_REAL_FINE), .cpu = chiClock(CHI_CLOCK_CPU) };
}

CHI_INL ChiTime chiTimeDelta(ChiTime a, ChiTime b) {
    return (ChiTime){
        .real = chiNanosDelta(a.real, b.real),
        .cpu  = chiNanosDelta(a.cpu, b.cpu),
    };
}

CHI_NEWTYPE(Trigger, _Atomic(bool))

CHI_INL void chiTrigger(ChiTrigger* t, bool set) {
    // Explicitly relaxed since we do not want any memory ordering constraints for triggers
    atomic_store_explicit(CHI_UNP(Trigger, t), set, memory_order_relaxed);
}

CHI_INL CHI_WU bool chiTriggered(ChiTrigger* t) {
    // Explicitly relaxed since we do not want any memory ordering constraints for triggers
    return atomic_load_explicit(CHI_UNP(Trigger, t), memory_order_relaxed);
}

CHI_INL CHI_WU bool chiTriggerExchange(ChiTrigger* t, bool set) {
    // Explicitly relaxed since we do not want any memory ordering constraints for triggers
    return atomic_exchange_explicit(CHI_UNP(Trigger, t), set, memory_order_relaxed);
}
