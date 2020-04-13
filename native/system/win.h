#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <errno.h>
#include <synchapi.h>
#include <unistd.h>
#include <windows.h>
#include "../utf16.h"

CHI_NEWTYPE(Task,   HANDLE)
CHI_NEWTYPE(Mutex,  SRWLOCK)
CHI_NEWTYPE(Cond,   CONDITION_VARIABLE)
CHI_NEWTYPE(File,   HANDLE)

typedef struct {
    HANDLE handle;
    char* current, *next;
} ChiDirStruct;
typedef ChiDirStruct* ChiDir;

typedef WINAPI unsigned (*ChiTaskRun)(void*);
#define CHI_TASK(n, a) static WINAPI unsigned n(void* a)

#define CHI_MUTEX_STATIC_INIT CHI_WRAP(Mutex, SRWLOCK_INIT)

#define CHI_FILE_STDIN  CHI_WRAP(File, GetStdHandle(STD_INPUT_HANDLE))
#define CHI_FILE_STDOUT CHI_WRAP(File, GetStdHandle(STD_OUTPUT_HANDLE))
#define CHI_FILE_STDERR CHI_WRAP(File, GetStdHandle(STD_ERROR_HANDLE))

CHI_INL void chiMutexInit(ChiMutex* m)            { InitializeSRWLock(CHI_UNP(Mutex, m)); }
CHI_INL void chiMutexDestroy(ChiMutex* CHI_UNUSED(m)) {}
CHI_INL void chiMutexLock(ChiMutex* m)            { AcquireSRWLockExclusive(CHI_UNP(Mutex, m)); }
CHI_INL void chiMutexUnlock(ChiMutex* m)          { ReleaseSRWLockExclusive(CHI_UNP(Mutex, m)); }
CHI_INL bool chiMutexTryLock(ChiMutex* m)         { return !!TryAcquireSRWLockExclusive(CHI_UNP(Mutex, m)); }
CHI_INL void chiCondInit(ChiCond* c)              { InitializeConditionVariable(CHI_UNP(Cond, c)); }
CHI_INL void chiCondDestroy(ChiCond* CHI_UNUSED(c)) {}
CHI_INL void chiCondSignal(ChiCond* c)            { WakeConditionVariable(CHI_UNP(Cond, c)); }
CHI_INL void chiCondWait(ChiCond* c, ChiMutex* m) { SleepConditionVariableSRW(CHI_UNP(Cond, c), CHI_UNP(Mutex, m), INFINITE, 0); }

CHI_UNITTYPE(ChiVirtMem)
CHI_INTERN CHI_WU bool chiVirtReserve(ChiVirtMem*, void*, size_t);
CHI_INTERN CHI_WU bool chiVirtCommit(ChiVirtMem*, void*, size_t, bool);
CHI_INTERN void chiVirtDecommit(ChiVirtMem*, void*, size_t);
CHI_INTERN CHI_WU void* chiVirtAlloc(void*, size_t, bool);
CHI_INTERN void chiVirtFree(void*, size_t);

CHI_INTERN bool chiErrnoString(char*, size_t);
CHI_INTERN void chiTimerInstall(ChiTimer*);
CHI_INTERN void chiTimerRemove(ChiTimer*);
CHI_INTERN void chiSigInstall(ChiSigHandler*);
CHI_INTERN void chiSigRemove(ChiSigHandler*);
CHI_INTERN CHI_WU ChiNanos chiClockMonotonicFine(void);
CHI_INL CHI_WU ChiNanos chiClockMonotonicFast(void) { return chiClockMonotonicFine(); }
CHI_INTERN CHI_WU ChiNanos chiClockCpu(void);
CHI_INTERN CHI_WU ChiNanos chiCondTimedWait(ChiCond*, ChiMutex*, ChiNanos);
CHI_INTERN CHI_WU ChiTask chiTaskCurrent(void);
CHI_INTERN CHI_WU ChiFile chiFileOpen(const char*);
CHI_INTERN CHI_WU bool chiFilePerm(const char*, int32_t);
CHI_INTERN CHI_WU uint32_t chiPhysProcessors(void);
CHI_INTERN CHI_WU uint64_t chiPhysMemory(void);
CHI_INTERN _Noreturn void chiTaskExit(void);
CHI_INTERN CHI_WU uint64_t chiFileSize(ChiFile);
CHI_INTERN void chiSystemStats(ChiSystemStats*);
CHI_INTERN void chiSystemSetup(void);
CHI_INTERN void chiTaskJoin(ChiTask);
CHI_INTERN CHI_WU ChiFile chiFileOpen(const char*);
CHI_INTERN CHI_WU ChiFile chiFileOpenRead(const char*);
CHI_INTERN CHI_WU bool chiFileReadOff(ChiFile, void*, size_t, uint64_t);
CHI_INTERN CHI_WU bool chiFileRead(ChiFile, void*, size_t, size_t*);
CHI_INTERN CHI_WU bool chiFileWrite(ChiFile, const void*, size_t);
CHI_INTERN CHI_INL void chiPager(void) {}
CHI_INTERN CHI_WU ChiDir chiDirOpen(const char*);
CHI_INTERN CHI_WU const char* chiDirEntry(ChiDir);
CHI_INTERN void chiDirClose(ChiDir);
CHI_INTERN bool chiWinErrorString(DWORD, char*, size_t);

CHI_INL CHI_WU ChiTask chiTaskTryCreate(ChiTaskRun run, void* arg) {
    return CHI_WRAP(Task, (HANDLE)_beginthreadex(0,   // security attributes
                                                 0,   // stack size
                                                 run, // thread function
                                                 arg, // argument to thread function
                                                 0,   // creation flags
                                                 0)); // thread identifier
}

CHI_INL void chiTaskCancel(ChiTask t) {
    CancelSynchronousIo(CHI_UN(Task, t));
}

CHI_INL void chiTaskClose(ChiTask t) {
    CloseHandle(CHI_UN(Task, t));
}

CHI_INL CHI_WU bool chiTaskEqual(ChiTask a, ChiTask b) {
    return CHI_UN(Task, a) == CHI_UN(Task, b);
}

CHI_INL CHI_WU bool chiTaskNull(ChiTask a) {
    return !CHI_UN(Task, a);
}

CHI_INL void chiFileClose(ChiFile file) {
    CloseHandle(CHI_UN(File, file));
}

CHI_INL CHI_WU bool chiFileNull(ChiFile file) {
    return !CHI_UN(File, file);
}

CHI_INL void chiTaskName(const char* CHI_UNUSED(name)) {
    // NOP since SetThreadDescription is only available on Windows 10 and newer
}

CHI_INL uint32_t chiPid(void) {
    return GetCurrentProcessId();
}

CHI_INL bool chiFileTerminal(ChiFile file) {
    return GetFileType(CHI_UN(File, file)) == FILE_TYPE_CHAR;
}

CHI_INL void chi_auto_chiFileClose(ChiFile* f) {
    if (!chiFileNull(*f))
        chiFileClose(*f);
}

CHI_INL CHI_WU ChiFile chiFileOpenFd(int CHI_UNUSED(fd)) {
    return CHI_WRAP(File, 0);
}

CHI_INL CHI_WU bool chiDirNull(ChiDir dir) {
    return !dir;
}

CHI_INL void chi_auto_chiDirClose(ChiDir* f) {
    if (!chiDirNull(*f))
        chiDirClose(*f);
}
