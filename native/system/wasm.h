#include <wasm.h>

CHI_NEWTYPE(File, uint32_t)
CHI_UNITTYPE(ChiMutex)
CHI_UNITTYPE(ChiCond)

#define CHI_FILE_STDIN  CHI_WRAP(File, 0)
#define CHI_FILE_STDOUT CHI_WRAP(File, 1)
#define CHI_FILE_STDERR CHI_WRAP(File, 2)

#define CHI_MUTEX_STATIC_INIT (ChiMutex){}

CHI_UNITTYPE(ChiVirtMem)
CHI_INL CHI_WU bool chiVirtReserve(ChiVirtMem* CHI_UNUSED(mem), void* CHI_UNUSED(p), size_t CHI_UNUSED(s)) { return true; }
CHI_INTERN CHI_WU bool chiVirtCommit(ChiVirtMem*, void*, size_t, bool);
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
CHI_INL ChiNanos chiCondTimedWait(ChiCond* CHI_UNUSED(c), ChiMutex* CHI_UNUSED(m), ChiNanos CHI_UNUSED(t)) { CHI_BUG("Function not available"); }
CHI_INL uint64_t chiPhysMemory(void) { return CHI_GiB(1); }
CHI_INL uint32_t chiPid(void) { return 0; }
CHI_WU CHI_INL bool chiFileTerminal(ChiFile CHI_UNUSED(file)) { return false; }
CHI_WU CHI_INL bool chiFilePerm(const char* CHI_UNUSED(file), int32_t CHI_UNUSED(mode)) { return false; }
CHI_INL void chiPager(void) {}

CHI_INL CHI_WU ChiNanos chiClockFine(void) {
    uint64_t t;
    wasm_clock(&t);
    return (ChiNanos){t};
}

CHI_INL CHI_WU ChiNanos chiClockFast(void) { return chiClockFine(); }

CHI_WU CHI_INL ChiFile chiFileOpen(const char* file) {
    return CHI_WRAP(File, wasm_sink_open(file));
}

CHI_WU CHI_INL ChiFile chiFileOpenFd(int CHI_UNUSED(fd)) {
    return CHI_WRAP(File, UINT32_MAX);
}

CHI_WU CHI_INL bool chiFileWrite(ChiFile file, const void* buf, size_t size) {
    wasm_sink_write_all(CHI_UN(File, file), buf, size);
    return true;
}

CHI_INL void chiFileClose(ChiFile file) {
    wasm_sink_close(CHI_UN(FIle, file));
}

CHI_WU CHI_INL bool chiFileNull(ChiFile file) { return CHI_UN(File, file) == UINT32_MAX; }

CHI_INTERN _Noreturn void chiWasmContinue(void);
