#include <sandbox.h>

CHI_NEWTYPE(Task,    int)
CHI_NEWTYPE(Mutex,   int)
CHI_NEWTYPE(Cond,    int)
CHI_NEWTYPE(RWLock,  int)

CHI_INL void chiMutexInit(ChiMutex* CHI_UNUSED(m))            {}
CHI_INL void chiRWLockInit(ChiRWLock* CHI_UNUSED(l))          {}
CHI_INL void chiRWLockDestroy(ChiRWLock* CHI_UNUSED(l))       {}
CHI_INL void chiRWLockWrite(ChiRWLock* CHI_UNUSED(l))         {}
CHI_INL void chiRWLockRead(ChiRWLock* CHI_UNUSED(l))          {}
CHI_INL void chiRWUnlockRead(ChiRWLock* CHI_UNUSED(l))        {}
CHI_INL void chiRWUnlockWrite(ChiRWLock* CHI_UNUSED(l))       {}
CHI_INL void chiMutexDestroy(ChiMutex* CHI_UNUSED(m))         {}
CHI_INL bool chiMutexTryLock(ChiMutex* CHI_UNUSED(m))         { return true; }
CHI_INL void chiMutexLock(ChiMutex* CHI_UNUSED(m))            {}
CHI_INL void chiMutexUnlock(ChiMutex* CHI_UNUSED(m))          {}
CHI_INL void chiCondInit(ChiCond* CHI_UNUSED(c))              {}
CHI_INL void chiCondDestroy(ChiCond* CHI_UNUSED(c))           {}
CHI_INL void chiCondSignal(ChiCond* CHI_UNUSED(c))            {}
CHI_INL void chiCondBroadcast(ChiCond* CHI_UNUSED(c))         {}
CHI_INL ChiTask chiTaskCurrent(void) { return (ChiTask){0}; }
CHI_INL ChiTask chiTaskTryCreate(ChiTaskRun CHI_UNUSED(run), void* CHI_UNUSED(arg)) { return (ChiTask){0}; }
CHI_INL void chiTaskYield(void) {}
CHI_INL bool chiTaskEqual(ChiTask CHI_UNUSED(a), ChiTask CHI_UNUSED(b)) { return true; }
CHI_INL bool chiTaskNull(ChiTask CHI_UNUSED(a)) { return true; }
CHI_INL void chiTaskName(const char* CHI_UNUSED(name)) {}
CHI_INL void chiActivity(ChiActivity* a) { CHI_CLEAR(a); }
CHI_INL uint32_t chiPhysProcessors(void) { return 1; }
CHI_INL void* chiVirtAlloc(void* p, size_t CHI_UNUSED(s), int32_t CHI_UNUSED(f)) { return p; }
CHI_INL void chiVirtFree(void* CHI_UNUSED(p), size_t CHI_UNUSED(s)) {}
CHI_INL void chiSystemSetup(void) {}
CHI_INL void chiCondWait(ChiCond* CHI_UNUSED(c), ChiMutex* CHI_UNUSED(m)) { CHI_BUG("Function not available"); }
CHI_INL void chiTaskCancel(ChiTask CHI_UNUSED(t)) { CHI_BUG("Function not available"); }
CHI_INL void chiTaskJoin(ChiTask CHI_UNUSED(t)) { CHI_BUG("Function not available"); }
CHI_INL void chiTaskClose(ChiTask CHI_UNUSED(t)) { CHI_BUG("Function not available"); }
CHI_INL ChiTask chiTaskCreate(ChiTaskRun CHI_UNUSED(run), void* CHI_UNUSED(arg)) { CHI_BUG("Function not available"); }
CHI_INL ChiNanos chiClock(ChiClock CHI_UNUSED(c)) { return (ChiNanos){ sb_clock_monotonic() }; }
CHI_INL _Noreturn void chiTaskExit(void) { exit(0); }
CHI_INL uint64_t chiPhysMemory(void) { return sb_info->heap.size; }

CHI_WU ChiNanos chiCondTimedWait(ChiCond*, ChiMutex*, ChiNanos);
CHI_WU uint32_t chiPid(void);
