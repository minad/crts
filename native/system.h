#pragma once

#include <stdlib.h>
#include "list.h"
#include "event/utils.h"

typedef struct ChiTimer_ ChiTimer;
typedef struct ChiSigHandler_ ChiSigHandler;
typedef void (*ChiDestructor)(void*);
typedef void (*ChiTaskRun)(void*);

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
    CHI_MEMMAP_HUGE      = 1,
    CHI_MEMMAP_NOHUGE    = 2,
    CHI_MEMMAP_NORESERVE = 4,
};

enum {
    CHI_FILE_READABLE   = 1,
    CHI_FILE_EXECUTABLE = 2,
};

void chiTimerInstall(ChiTimer*);
void chiTimerRemove(ChiTimer*);
void chiSigInstall(ChiSigHandler*);
void chiSigRemove(ChiSigHandler*);
CHI_WU void* chiTaskLocal(size_t, ChiDestructor);
bool chiErrorString(char*, size_t);

#if CHI_ENV_ENABLED
char* chiGetEnv(const char*);
#else
CHI_INL char* chiGetEnv(const char* CHI_UNUSED(v)) { return 0; }
#endif

#if CHI_PAGER_ENABLED
void chiPager(void);
#else
CHI_INL void chiPager(void) {}
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

struct ChiSigHandler_ {
    void    (*handler)(ChiSigHandler*);
    ChiList _list;
    ChiSig  sig;
};

#define CHI_DEFINE_AUTO_LOCK(t, n, lock, unlock)        \
    CHI_NEWTYPE(Lock##t##n, t*)                         \
    CHI_INL ChiLock##t##n _chiLock##t##n(t* x) {        \
        lock(x);                                        \
        return (ChiLock##t##n){ x };                    \
    }                                                   \
    CHI_INL void chi_auto_unlock##t##n(ChiLock##t##n* l) { \
        unlock(CHI_UN(Lock##t##n, *l));                 \
    }

#define _CHI_AUTO_LOCK(l, t, n, x) CHI_AUTO(l, _chiLock##t##n(x), unlock##t##n); CHI_NOWARN_UNUSED(l);
#define CHI_AUTO_LOCK(t, n, m)     _CHI_AUTO_LOCK(CHI_GENSYM, t, n, m)

CHI_DEFINE_AUTO_LOCK(ChiMutex, , chiMutexLock, chiMutexUnlock)
CHI_DEFINE_AUTO_LOCK(ChiRWLock, r, chiRWLockRead, chiRWUnlockRead)
CHI_DEFINE_AUTO_LOCK(ChiRWLock, w, chiRWLockWrite, chiRWUnlockWrite)

#define CHI_LOCK(m) CHI_AUTO_LOCK(ChiMutex, , m)
#define CHI_LOCK_READ(m) CHI_AUTO_LOCK(ChiRWLock, r, m)
#define CHI_LOCK_WRITE(m) CHI_AUTO_LOCK(ChiRWLock, w, m)

CHI_INL ChiTime chiTime(void) {
    return (ChiTime){ .real = chiClock(CHI_CLOCK_REAL_FINE), .cpu = chiClock(CHI_CLOCK_CPU) };
}

CHI_INL ChiTime chiTimeDelta(ChiTime a, ChiTime b) {
    return (ChiTime){
        .real = chiNanosDelta(a.real, b.real),
        .cpu  = chiNanosDelta(a.cpu, b.cpu),
    };
}
