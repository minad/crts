#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <synchapi.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../utf16.h"

CHI_NEWTYPE(Task,   HANDLE)
CHI_NEWTYPE(Mutex,  CRITICAL_SECTION)
CHI_NEWTYPE(Cond,   CONDITION_VARIABLE)
CHI_NEWTYPE(RWLock, SRWLOCK)

CHI_WU bool chiWinErrorString(uint32_t, char*, size_t);
void chiWinErr(const char*);
void chiWinSetErr(void);

CHI_INL void chiMutexInit(ChiMutex* m)            { InitializeCriticalSectionAndSpinCount(CHI_UNP(Mutex, m), 1000); }
CHI_INL void chiMutexDestroy(ChiMutex* m)         { DeleteCriticalSection(CHI_UNP(Mutex, m)); }
CHI_INL bool chiMutexTryLock(ChiMutex* m)         { return !!TryEnterCriticalSection(CHI_UNP(Mutex, m)); }
CHI_INL void chiMutexLock(ChiMutex* m)            { EnterCriticalSection(CHI_UNP(Mutex, m)); }
CHI_INL void chiMutexUnlock(ChiMutex* m)          { LeaveCriticalSection(CHI_UNP(Mutex, m)); }
CHI_INL void chiCondInit(ChiCond* c)              { InitializeConditionVariable(CHI_UNP(Cond, c)); }
CHI_INL void chiCondDestroy(ChiCond* c)           { CHI_NOWARN_UNUSED(c); }
CHI_INL void chiCondSignal(ChiCond* c)            { WakeConditionVariable(CHI_UNP(Cond, c)); }
CHI_INL void chiCondBroadcast(ChiCond* c)         { WakeAllConditionVariable(CHI_UNP(Cond, c)); }
CHI_INL void chiCondWait(ChiCond* c, ChiMutex* m) { SleepConditionVariableCS(CHI_UNP(Cond, c), CHI_UNP(Mutex, m), INFINITE); }
CHI_INL void chiRWLockInit(ChiRWLock* l)          { InitializeSRWLock(CHI_UNP(RWLock, l)); }
CHI_INL void chiRWLockDestroy(ChiRWLock* l)       { CHI_NOWARN_UNUSED(l); }
CHI_INL void chiRWLockWrite(ChiRWLock* l)         { AcquireSRWLockExclusive(CHI_UNP(RWLock, l)); }
CHI_INL void chiRWLockRead(ChiRWLock* l)          { AcquireSRWLockShared(CHI_UNP(RWLock, l)); }
CHI_INL void chiRWUnlockWrite(ChiRWLock* l)       { ReleaseSRWLockExclusive(CHI_UNP(RWLock, l)); }
CHI_INL void chiRWUnlockRead(ChiRWLock* l)        { ReleaseSRWLockShared(CHI_UNP(RWLock, l)); }

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
CHI_WU uint32_t chiPhysProcessors(void);
CHI_WU uint32_t chiPid(void);
CHI_WU uint64_t chiPhysMemory(void);
CHI_WU void* chiVirtAlloc(void*, size_t, int32_t);
_Noreturn void chiTaskExit(void);
bool chiTerminal(int);
void chiActivity(ChiActivity*);
void chiSystemSetup(void);
void chiTaskCancel(ChiTask);
void chiTaskClose(ChiTask);
void chiTaskJoin(ChiTask);
void chiTaskName(const char*);
void chiTaskYield(void);
void chiVirtFree(void*, size_t);
