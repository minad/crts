#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

CHI_NEWTYPE(Task,    pthread_t)
CHI_NEWTYPE(Mutex,   pthread_mutex_t)
CHI_NEWTYPE(Cond,    pthread_cond_t)
CHI_NEWTYPE(RWLock,  pthread_rwlock_t)

#define _CHI_PTHREAD_CHECK(x)                           \
    ({                                                  \
        int _ret = (x);                                 \
        if (!CHI_DEFINED(NDEBUG) && _ret) {             \
            errno = _ret;                               \
            CHI_BUG("%s failed with error %m", #x);     \
        }                                               \
        _ret;                                           \
    })

CHI_INL void chiMutexInit(ChiMutex* m) {
    if (CHI_DEFINED(NDEBUG)) {
        pthread_mutex_init(CHI_UNP(Mutex, m), 0);
    } else {
        pthread_mutexattr_t attr;
        _CHI_PTHREAD_CHECK(pthread_mutexattr_init(&attr));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        _CHI_PTHREAD_CHECK(pthread_mutex_init(CHI_UNP(Mutex, m), &attr));
        pthread_mutexattr_destroy(&attr);
    }
}

CHI_INL void chiRWLockInit(ChiRWLock* l) {
    pthread_rwlock_init(CHI_UNP(RWLock, l), 0);
}

CHI_INL void chiMutexDestroy(ChiMutex* m)              { _CHI_PTHREAD_CHECK(pthread_mutex_destroy(CHI_UNP(Mutex, m))); }
CHI_INL bool chiMutexTryLock(ChiMutex* m)              { return !pthread_mutex_trylock(CHI_UNP(Mutex, m)); }
CHI_INL void chiMutexLock(ChiMutex* m)                 { _CHI_PTHREAD_CHECK(pthread_mutex_lock(CHI_UNP(Mutex, m))); }
CHI_INL void chiMutexUnlock(ChiMutex* m)               { _CHI_PTHREAD_CHECK(pthread_mutex_unlock(CHI_UNP(Mutex, m))); }
CHI_INL void chiCondInit(ChiCond* c)                   { _CHI_PTHREAD_CHECK(pthread_cond_init(CHI_UNP(Cond, c), 0)); }
CHI_INL void chiCondDestroy(ChiCond* c)                { _CHI_PTHREAD_CHECK(pthread_cond_destroy(CHI_UNP(Cond, c))); }
CHI_INL void chiCondSignal(ChiCond* c)                 { _CHI_PTHREAD_CHECK(pthread_cond_signal(CHI_UNP(Cond, c))); }
CHI_INL void chiCondBroadcast(ChiCond* c)              { _CHI_PTHREAD_CHECK(pthread_cond_broadcast(CHI_UNP(Cond, c))); }
CHI_INL void chiCondWait(ChiCond* c, ChiMutex* m)      { _CHI_PTHREAD_CHECK(pthread_cond_wait(CHI_UNP(Cond, c), CHI_UNP(Mutex, m))); }
CHI_INL void chiRWLockDestroy(ChiRWLock* l)            { _CHI_PTHREAD_CHECK(pthread_rwlock_destroy(CHI_UNP(RWLock, l))); }
CHI_INL void chiRWLockWrite(ChiRWLock* l)              { _CHI_PTHREAD_CHECK(pthread_rwlock_wrlock(CHI_UNP(RWLock, l))); }
CHI_INL void chiRWLockRead(ChiRWLock* l)               { _CHI_PTHREAD_CHECK(pthread_rwlock_rdlock(CHI_UNP(RWLock, l))); }
CHI_INL void chiRWUnlockRead(ChiRWLock* l)             { _CHI_PTHREAD_CHECK(pthread_rwlock_unlock(CHI_UNP(RWLock, l))); }
CHI_INL void chiRWUnlockWrite(ChiRWLock* l)            { _CHI_PTHREAD_CHECK(pthread_rwlock_unlock(CHI_UNP(RWLock, l))); }

CHI_WU ChiNanos chiClock(ChiClock);
CHI_WU ChiNanos chiCondTimedWait(ChiCond*, ChiMutex*, ChiNanos);
CHI_WU ChiTask chiTaskCreate(ChiTaskRun, void*);
CHI_WU ChiTask chiTaskCurrent(void);
CHI_WU ChiTask chiTaskTryCreate(ChiTaskRun, void*);
CHI_WU FILE* chiFileOpenRead(const char*);
CHI_WU FILE* chiFileOpenWrite(const char*);
CHI_WU FILE* chiOpenFd(int);
CHI_WU FILE* chiOpenPipe(const char*);
CHI_WU bool chiFilePerm(const char*, int32_t);
CHI_WU bool chiTaskEqual(ChiTask, ChiTask);
CHI_WU bool chiTaskNull(ChiTask);
CHI_WU bool chiTerminal(int);
CHI_WU uint32_t chiPhysProcessors(void);
CHI_WU uint32_t chiPid(void);
CHI_WU uint64_t chiPhysMemory(void);
CHI_WU void* chiVirtAlloc(void*, size_t, int32_t);
_Noreturn void chiTaskExit(void);
void chiActivity(ChiActivity*);
void chiSystemSetup(void);
void chiTaskCancel(ChiTask);
void chiTaskClose(ChiTask);
void chiTaskJoin(ChiTask);
void chiTaskName(const char*);
void chiTaskYield(void);
void chiVirtFree(void*, size_t);
