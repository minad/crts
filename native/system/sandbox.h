#include <sandbox.h>

CHI_NEWTYPE(File, uint32_t)
CHI_UNITTYPE(ChiMutex)
CHI_UNITTYPE(ChiCond)

#define CHI_FILE_STDIN  CHI_WRAP(File, SB_STREAM_IN)
#define CHI_FILE_STDOUT CHI_WRAP(File, SB_STREAM_OUT)
#define CHI_FILE_STDERR CHI_WRAP(File, SB_STREAM_ERR)

#define CHI_MUTEX_STATIC_INIT (ChiMutex){}

CHI_UNITTYPE(ChiVirtMem)
CHI_INL CHI_WU bool chiVirtReserve(ChiVirtMem* CHI_UNUSED(mem), void* CHI_UNUSED(p), size_t CHI_UNUSED(s)) { return true; }
CHI_INL CHI_WU bool chiVirtCommit(ChiVirtMem* CHI_UNUSED(mem), void* p, size_t CHI_UNUSED(s), bool CHI_UNUSED(huge)) { CHI_ASSERT(p); return p; }
CHI_INL void chiVirtDecommit(ChiVirtMem* CHI_UNUSED(mem), void* CHI_UNUSED(p), size_t CHI_UNUSED(s)) {}

CHI_INL void chiMutexInit(ChiMutex* CHI_UNUSED(m))            {}
CHI_INL void chiMutexDestroy(ChiMutex* CHI_UNUSED(m))         {}
CHI_INL bool chiMutexTryLock(ChiMutex* CHI_UNUSED(m))         { return true; }
CHI_INL void chiMutexLock(ChiMutex* CHI_UNUSED(m))            {}
CHI_INL void chiMutexUnlock(ChiMutex* CHI_UNUSED(m))          {}
CHI_INL void chiCondInit(ChiCond* CHI_UNUSED(c))              {}
CHI_INL void chiCondDestroy(ChiCond* CHI_UNUSED(c))           {}
CHI_INL void chiCondSignal(ChiCond* CHI_UNUSED(c))            {}
CHI_INL void chiSystemSetup(void) {}
CHI_INL void chiCondWait(ChiCond* CHI_UNUSED(c), ChiMutex* CHI_UNUSED(m)) { CHI_BUG("Function not available"); }
CHI_INL CHI_WU ChiNanos chiClockMonotonicFine(void) { return (ChiNanos){ sb_clock_monotonic() }; }
CHI_INL CHI_WU ChiNanos chiClockMonotonicFast(void) { return chiClockMonotonicFine(); }
CHI_INL CHI_WU ChiNanos chiClockCpu(void) {  return chiClockMonotonicFine(); }
CHI_INL uint64_t chiPhysMemory(void) { return sb_info->heap.size; }
CHI_INTERN CHI_WU ChiNanos chiCondTimedWait(ChiCond*, ChiMutex*, ChiNanos);
CHI_INL CHI_WU uint32_t chiPid(void) { return 0; }
CHI_WU CHI_INL bool chiFileTerminal(ChiFile CHI_UNUSED(file)) { return false; }
CHI_WU CHI_INL bool chiFilePerm(const char* CHI_UNUSED(file), int32_t CHI_UNUSED(mode)) { return false; }
CHI_INL void chiPager(void) {}

CHI_WU CHI_INL ChiFile chiFileOpen(const char* CHI_UNUSED(name)) {
    return CHI_WRAP(File, UINT32_MAX);
}

CHI_WU CHI_INL ChiFile chiFileOpenFd(int fd) {
    uint32_t id = (uint32_t)fd;
    if (id >= sb_info->stream_count || !(sb_info->stream[id].mode & SB_MODE_WRITE))
        id = UINT32_MAX;
    return CHI_WRAP(File, id);
}

CHI_WU CHI_INL bool chiFileWrite(ChiFile file, const void* buf, size_t size) {
    sb_stream_write_all(CHI_UN(File, file), buf, size);
    return true;
}

CHI_WU CHI_INL bool chiFileNull(ChiFile file) { return CHI_UN(File, file) == UINT32_MAX; }
CHI_INL void chiFileClose(ChiFile CHI_UNUSED(file)) {}
