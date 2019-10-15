#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

CHI_NEWTYPE(Dir,   DIR*)
CHI_NEWTYPE(Task,  pthread_t)
CHI_NEWTYPE(Mutex, pthread_mutex_t)
CHI_NEWTYPE(Cond,  pthread_cond_t)
CHI_NEWTYPE(File,  int)

#define CHI_MUTEX_STATIC_INIT CHI_WRAP(Mutex, PTHREAD_MUTEX_INITIALIZER)

#define CHI_FILE_STDIN  CHI_WRAP(File, STDIN_FILENO)
#define CHI_FILE_STDOUT CHI_WRAP(File, STDOUT_FILENO)
#define CHI_FILE_STDERR CHI_WRAP(File, STDERR_FILENO)

#ifdef NDEBUG
#  define _CHI_PTHREAD_CHECK(x) x
#else
#  define _CHI_PTHREAD_CHECK(x)                           \
    ({                                                  \
        int _ret = (x);                                 \
        if (_ret) {                                     \
            errno = _ret;                               \
            CHI_BUG("%s failed with error %m", #x);     \
        }                                               \
        _ret;                                           \
    })
#endif

CHI_INL void chiMutexInit(ChiMutex* m) {
#ifdef NDEBUG
    pthread_mutex_init(CHI_UNP(Mutex, m), 0);
#else
    pthread_mutexattr_t attr;
    _CHI_PTHREAD_CHECK(pthread_mutexattr_init(&attr));
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    _CHI_PTHREAD_CHECK(pthread_mutex_init(CHI_UNP(Mutex, m), &attr));
    pthread_mutexattr_destroy(&attr);
#endif
}

CHI_INL void chiMutexDestroy(ChiMutex* m)              { _CHI_PTHREAD_CHECK(pthread_mutex_destroy(CHI_UNP(Mutex, m))); }
CHI_INL bool chiMutexTryLock(ChiMutex* m)              { return !pthread_mutex_trylock(CHI_UNP(Mutex, m)); }
CHI_INL void chiMutexLock(ChiMutex* m)                 { _CHI_PTHREAD_CHECK(pthread_mutex_lock(CHI_UNP(Mutex, m))); }
CHI_INL void chiMutexUnlock(ChiMutex* m)               { _CHI_PTHREAD_CHECK(pthread_mutex_unlock(CHI_UNP(Mutex, m))); }
CHI_INL void chiCondInit(ChiCond* c)                   { _CHI_PTHREAD_CHECK(pthread_cond_init(CHI_UNP(Cond, c), 0)); }
CHI_INL void chiCondDestroy(ChiCond* c)                { _CHI_PTHREAD_CHECK(pthread_cond_destroy(CHI_UNP(Cond, c))); }
CHI_INL void chiCondSignal(ChiCond* c)                 { _CHI_PTHREAD_CHECK(pthread_cond_signal(CHI_UNP(Cond, c))); }
CHI_INL void chiCondWait(ChiCond* c, ChiMutex* m)      { _CHI_PTHREAD_CHECK(pthread_cond_wait(CHI_UNP(Cond, c), CHI_UNP(Mutex, m))); }

CHI_INL CHI_WU uint32_t chiPid(void) {
    return (uint32_t)getpid();
}

CHI_INL CHI_WU ChiTask chiTaskCurrent(void) {
    return CHI_WRAP(Task, pthread_self());
}

CHI_INL _Noreturn void chiTaskExit(void) {
    pthread_exit(0);
}

CHI_INL void chiTaskJoin(ChiTask t) {
    _CHI_PTHREAD_CHECK(pthread_join(CHI_UN(Task, t), 0));
}

CHI_INL void chiTaskClose(ChiTask CHI_UNUSED(t)) {
}

CHI_INL CHI_WU bool chiTaskEqual(ChiTask a, ChiTask b) {
    return pthread_equal(CHI_UN(Task, a), CHI_UN(Task, b));
}

CHI_INL CHI_WU bool chiTaskNull(ChiTask a) {
    ChiTask b;
    CHI_ZERO_STRUCT(&b);
    return chiTaskEqual(a, b);
}

CHI_INL CHI_WU bool chiFileWrite(ChiFile file, const void* buf, size_t size) {
    return write(CHI_UN(File, file), buf, size) == (ssize_t)size;
}

CHI_INL void chiFileClose(ChiFile file) {
    close(CHI_UN(File, file));
}

CHI_INL CHI_WU bool chiFileNull(ChiFile file) {
    return CHI_UN(File, file) < 0;
}

CHI_INL CHI_WU ChiFile chiFileOpen(const char* file) {
    return CHI_WRAP(File, open(file, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644));
}

CHI_INL CHI_WU ChiFile chiFileOpenRead(const char* file) {
    return CHI_WRAP(File, open(file, O_RDONLY | O_CLOEXEC));
}

CHI_INL CHI_WU ChiFile chiFileOpenFd(int fd) {
    return CHI_WRAP(File, dup(fd));
}

CHI_INL CHI_WU bool chiFileReadOff(ChiFile file, void* buf, size_t size, uint64_t off) {
    _Static_assert(sizeof (off_t) == 8, "Large files are not supported");
    return pread(CHI_UN(File, file), buf, size, (off_t)off) == (ssize_t)size;
}

CHI_INL CHI_WU bool chiFileRead(ChiFile file, void* buf, size_t size, size_t* nread) {
    ssize_t res = read(CHI_UN(File, file), buf, size);
    if (res >= 0) {
        *nread = (size_t)res;
        return true;
    }
    return false;
}

CHI_INL CHI_WU uint64_t chiFileSize(ChiFile file) {
    struct stat st;
    return fstat(CHI_UN(File, file), &st) < 0 ? UINT64_MAX : (uint64_t)st.st_size;
}

CHI_INL CHI_WU ChiDir chiDirOpen(const char* path) {
    return CHI_WRAP(Dir, opendir(path));
}

CHI_INL void chiDirClose(ChiDir dir) {
    closedir(CHI_UN(Dir, dir));
}

CHI_INL CHI_WU const char* chiDirEntry(ChiDir dir) {
    struct dirent* d = readdir(CHI_UN(Dir, dir));
    return d ? d->d_name : 0;
}

CHI_INL CHI_WU bool chiDirNull(ChiDir dir) {
    return !CHI_UN(Dir, dir);
}

typedef struct {
    void* _ptr;
    size_t _size;
    int _fd;
} ChiVirtMem;

typedef void* (*ChiTaskRun)(void*);
#define CHI_TASK(n, a) static void* n(void* a)

CHI_INTERN CHI_WU bool chiVirtReserve(ChiVirtMem*, void*, size_t);
CHI_INTERN CHI_WU bool chiVirtCommit(ChiVirtMem*, void*, size_t, bool);
CHI_INTERN void chiVirtDecommit(ChiVirtMem*, void*, size_t);
CHI_INTERN CHI_WU void* chiVirtAlloc(void*, size_t, bool);
CHI_INTERN void chiVirtFree(void*, size_t);

CHI_INTERN CHI_WU bool chiErrnoString(char*, size_t);
CHI_INTERN void chiTimerInstall(ChiTimer*);
CHI_INTERN void chiTimerRemove(ChiTimer*);
CHI_INTERN void chiSigInstall(ChiSigHandler*);
CHI_INTERN void chiSigRemove(ChiSigHandler*);
CHI_INTERN CHI_WU ChiNanos chiClock(ChiClock);
CHI_INTERN CHI_WU ChiNanos chiCondTimedWait(ChiCond*, ChiMutex*, ChiNanos);
CHI_INTERN CHI_WU ChiTask chiTaskTryCreate(ChiTaskRun, void*);
CHI_INTERN CHI_WU bool chiFilePerm(const char*, int32_t);
CHI_INTERN CHI_WU bool chiFileTerminal(ChiFile);
CHI_INTERN CHI_WU uint32_t chiPhysProcessors(void);
CHI_INTERN CHI_WU uint64_t chiPhysMemory(void);
CHI_INTERN void chiSystemStats(ChiSystemStats*);
CHI_INTERN void chiSystemSetup(void);
CHI_INTERN void chiTaskCancel(ChiTask);
CHI_INTERN void chiTaskName(const char*);
CHI_INTERN void chiPager(void);

CHI_INL void chi_auto_chiFileClose(ChiFile* f) {
    if (!chiFileNull(*f))
        chiFileClose(*f);
}

CHI_INL void chi_auto_chiDirClose(ChiDir* f) {
    if (!chiDirNull(*f))
        chiDirClose(*f);
}
