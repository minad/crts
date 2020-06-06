#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include "../strutil.h"
#include "../num.h"
#include "interrupt.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#  include <pthread_np.h>
#  define pthread_setname_np pthread_set_name_np
#elif defined(__MACH__)
#  define pthread_setname_np(t, n) pthread_setname_np(n)
#elif defined (__linux__)
#  include <sched.h>
#  include <sys/syscall.h>
#  define pthread_main_np() (getpid() == syscall(SYS_gettid))
#endif

#ifdef MADV_HUGEPAGE
#  define _CHI_MADV_HUGE   MADV_HUGEPAGE
#  define _CHI_MADV_NOHUGE MADV_NOHUGEPAGE
#else
#  define _CHI_MADV_HUGE   0
#  define _CHI_MADV_NOHUGE 0
#endif

#if defined(CLOCK_MONOTONIC_COARSE)
#  define CHI_CLOCK_MONOTONIC_FAST CLOCK_MONOTONIC_COARSE
#elif defined(CLOCK_MONOTONIC_FAST)
#  define CHI_CLOCK_MONOTONIC_FAST CLOCK_MONOTONIC_FAST
#else
#  define CHI_CLOCK_MONOTONIC_FAST CLOCK_MONOTONIC
#endif

static ChiTask dispatcherTask;

static struct timespec nanosToTimespec(ChiNanos ns) {
    return (struct timespec) { .tv_sec = (long)(CHI_UN(Nanos, ns) / 1000000000UL),
            .tv_nsec = (long)(CHI_UN(Nanos, ns) % 1000000000UL) };
}

static ChiNanos timespecToNanos(struct timespec tv) {
    return chiNanos((uint64_t)tv.tv_sec * 1000000000 + (uint64_t)tv.tv_nsec);
}

static ChiNanos timevalToNanos(struct timeval tv) {
    return chiNanos((uint64_t)tv.tv_sec * 1000000000 + (uint64_t)tv.tv_usec * 1000);
}

static ChiSig convertPosixSig(int sig) {
    switch (sig) {
    case SIGQUIT: return CHI_SIG_DUMP;
    case SIGINT:  return CHI_SIG_USERINTERRUPT;
    default:      CHI_BUG("Wrong signal");
    }
}

static void dispatchPosixSig(int sig) {
    if (sig == SIGQUIT || sig == SIGINT)
        dispatchSig(convertPosixSig(sig));
}

static void setSigMask(int how, sigset_t* set) {
    _CHI_PTHREAD_CHECK(pthread_sigmask(how, set, 0));
}

static void blockSignals(void) {
    sigset_t block;
    sigfillset(&block);
    sigdelset(&block, SIGABRT);
    sigdelset(&block, SIGBUS);
    sigdelset(&block, SIGFPE);
    sigdelset(&block, SIGILL);
    sigdelset(&block, SIGSEGV);
    sigdelset(&block, SIGSYS);
    sigdelset(&block, SIGTRAP);
    sigdelset(&block, SIGTSTP);
    sigdelset(&block, SIGTERM);
    sigdelset(&block, SIGHUP);
    sigdelset(&block, SIGPROF);
    sigdelset(&block, SIGPIPE);
    setSigMask(SIG_SETMASK, &block);
}

static long sysconf_err(int c) {
    long n = sysconf(c);
    if (n <= 0)
        chiSysErr("sysconf");
    return n;
}

#ifdef __linux__
#  include "dispatch/epoll.h"
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__MACH__)
#  include "dispatch/kqueue.h"
#else
#  include "dispatch/signal.h"
#endif

void chiSystemSetup(void) {
    CHI_ASSERT_ONCE;
    blockSignals();

    struct sigaction sa = { .sa_handler = SIG_IGN };
    if (sigaction(SIGPIPE, &sa, 0))
        chiSysErr("sigaction");

    dispatcherTask = chiTaskTryCreate(dispatcherRun, 0);
    if (chiTaskNull(dispatcherTask))
        chiSysErr("chiTaskTryCreate");
}

static ChiNanos getClock(clockid_t clock) {
    struct timespec ts;
    if (clock_gettime(clock, &ts))
        chiSysErr("clock_gettime");
    return timespecToNanos(ts);
}

ChiNanos chiClockFine(void) {
    return getClock(CLOCK_MONOTONIC);
}

ChiNanos chiClockFast(void) {
    return getClock(CHI_CLOCK_MONOTONIC_FAST);
}

ChiNanos chiCondTimedWait(ChiCond* c, ChiMutex* m, ChiNanos ns) {
    ChiNanos begin = getClock(CLOCK_REALTIME);
    struct timespec ts = nanosToTimespec(chiNanosAdd(begin, ns));
    int err = pthread_cond_timedwait(CHI_UNP(Cond, c), CHI_UNP(Mutex, m), &ts);
    if (err != ETIMEDOUT) {
#ifndef NDEBUG
        _CHI_PTHREAD_CHECK(err);
#endif
    }
    return chiNanosDelta(getClock(CLOCK_REALTIME), begin);
}

ChiTask chiTaskTryCreate(ChiTaskRun run, void* arg) {
    ChiTask t;
    int ret = pthread_create(CHI_UNP(Task, &t), 0, run, arg);
    if (ret) {
        errno = ret;
        CHI_ZERO_STRUCT(&t);
    }
    return t;
}

void chiTaskCancel(ChiTask t) {
    pthread_kill(CHI_UN(Task, t), SIGPIPE);
}

void chiTaskName(const char* name) {
    // don't change name of process to allow killall
    if (!pthread_main_np())
        pthread_setname_np(pthread_self(), name);
}

static void madvise_err(void* p, size_t s, int flags) {
    if (flags && madvise(p, s, flags))
        chiSysErr("madvise");
}

bool chiVirtReserve(ChiVirtMem* mem, void* ptr, size_t size) {
    CHI_ASSERT(ptr);
    CHI_ASSERT(size);

    /* Use a temporary file for the memory mapping.
     * In contrast to an anonymous mapping, this allows
     * to reserve the complete heap without overcommiting.
     * Note that the file will never be written to,
     * since the mapping is PROT_NONE.
     */
    char path[] = "/tmp/chili-heap.XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return false;
    unlink(path);

    void* res = mmap(ptr, size,
                     PROT_NONE,
                     MAP_PRIVATE,
                     fd, 0);

    // Check that we got the right address
    if (res != ptr) {
        if (res != MAP_FAILED)
            chiErr("Could not reserve memory %p - %p", ptr, (uint8_t*)ptr + size);
        close(fd);
        return false;
    }

    mem->_ptr = ptr;
    mem->_size = size;
    mem->_fd = fd;
    return true;
}

bool chiVirtCommit(ChiVirtMem* mem, void* ptr, size_t size, bool huge) {
    CHI_ASSERT(ptr);
    CHI_ASSERT(size);
    CHI_ASSERT((uint8_t*)mem->_ptr <= (uint8_t*)ptr);
    CHI_ASSERT((uint8_t*)ptr + size <= (uint8_t*)mem->_ptr + mem->_size);
    CHI_NOWARN_UNUSED(mem);

    // Replace exiting reserved mapping with an anonymous mapping
    if (mmap(ptr, size,
             PROT_READ | PROT_WRITE,
             MAP_PRIVATE |
             MAP_FIXED |
             MAP_ANONYMOUS,
             -1, 0) == MAP_FAILED)
        return false;

    madvise_err(ptr, size, huge ? _CHI_MADV_HUGE : _CHI_MADV_NOHUGE);
    return true;
}

void chiVirtDecommit(ChiVirtMem* mem, void* ptr, size_t size) {
    CHI_ASSERT(ptr);
    CHI_ASSERT(size);
    CHI_ASSERT((uint8_t*)mem->_ptr <= (uint8_t*)ptr);
    CHI_ASSERT((uint8_t*)ptr + size <= (uint8_t*)mem->_ptr + mem->_size);

    // Replace exiting mapping by protected heap reservation mapping
    if (mmap(ptr, size,
             PROT_NONE,
             MAP_PRIVATE |
             MAP_FIXED,
             mem->_fd,
             (off_t)((uint8_t*)ptr - (uint8_t*)mem->_ptr)) == MAP_FAILED)
        chiSysErr("mmap");
}

void* chiVirtAlloc(void* ptr, size_t size, bool huge) {
    ptr = mmap(ptr, size,
               PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS |
               MAP_PRIVATE,
               -1, 0);
    if (ptr == MAP_FAILED)
        return 0;
    madvise_err(ptr, size, huge ? _CHI_MADV_HUGE : _CHI_MADV_NOHUGE);
    return ptr;
}

void chiVirtFree(void* p, size_t s) {
    if (munmap(p, s))
        chiSysErr("munmap");
}

uint32_t chiPhysProcessors(void) {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) < 0)
        chiSysErr("sched_getaffinity");
    uint32_t n = 0;
    for (uint32_t i = 0; i < CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &set))
                ++n;
    }
    return n;
#else
    return (uint32_t)sysconf_err(_SC_NPROCESSORS_ONLN);
#endif
}

static size_t pageSize(void) {
    static size_t pageSize = 0;
    if (!pageSize)
        pageSize = (size_t)sysconf_err(_SC_PAGESIZE);
    return pageSize;
}

uint64_t chiPhysMemory(void) {
    return CHI_FLOOR((uint64_t)sysconf_err(_SC_PHYS_PAGES) * pageSize(), CHI_MiB(1));
}

bool chiFilePerm(const char* file, int32_t perm) {
    int flags = F_OK;
    if (perm & CHI_FILE_READABLE)
        flags |= R_OK;
    if (perm & CHI_FILE_EXECUTABLE)
        flags |= X_OK;
    return !access(file, flags);
}

static size_t getCurrentRSS(void) {
#ifdef __linux__
    int fd = open("/proc/self/statm", O_RDONLY);
    if (fd < 0)
        return 0;
    char buf[64];
    ssize_t n = read(fd, buf, sizeof (buf) - 1);
    close(fd);
    buf[CHI_MAX(n, 0)] = 0;
    const char* p = strchr(buf, ' ');
    if (!p)
        return 0;
    ++p;
    uint64_t rss;
    return chiReadUInt64(&rss, &p) ? (size_t)(rss * pageSize()) : 0;
#endif
    return 0;
}

void chiSystemStats(ChiSystemStats* a) {
    struct rusage r;
    if (getrusage(RUSAGE_SELF, &r) < 0)
        chiSysErr("getrusage");
    a->cpuTimeUser = timevalToNanos(r.ru_utime);
    a->cpuTimeSystem = timevalToNanos(r.ru_stime);
    a->pageFault = (uint64_t)r.ru_minflt;
    a->involuntaryContextSwitch = (uint64_t)r.ru_nivcsw;
    a->voluntaryContextSwitch = (uint64_t)r.ru_nvcsw;
    a->maxResidentSize = (size_t)r.ru_maxrss * 1024;
    a->currResidentSize = getCurrentRSS();
    if (!a->currResidentSize)
        a->currResidentSize = a->maxResidentSize; // Fallback
}

void chiPager(void) {
    if (!CHI_PAGER_ENABLED || !chiFileTerminal(CHI_FILE_STDOUT))
        return;
    CHI_AUTO_FREE(pagerEnv, chiGetEnv("CHI_PAGER"));
    const char* pager = pagerEnv ? pagerEnv : CHI_PAGER;
    if (!pager[0] || streq(pager, "cat"))
        return;
    int fd[2];
    if (pipe(fd))
        chiSysErr("pipe");
    pid_t pid = fork();
    if (pid < 0)
        chiSysErr("fork");
    if (pid) { // parent
        if (dup2(fd[0], STDIN_FILENO) < 0)
            chiSysErr("dup2");
        close(fd[0]);
        close(fd[1]);
        execl("/bin/sh", "sh", "-c", pager, NULL);
        chiSysErr("execl");
    }
    // child
    if (dup2(fd[1], STDOUT_FILENO) < 0)
        chiSysErr("dup2");
    close(fd[0]);
    close(fd[1]);
}

bool chiFileTerminal(ChiFile file) {
    const char* term = CHI_ENV_ENABLED ? getenv("TERM") : 0;
    if (term && streq(term, "dumb"))
        return false;
    return !!isatty(CHI_UN(File, file));
}

#if CHI_ENV_ENABLED
char* chiGetEnv(const char* var) {
    const char* val = getenv(var);
    return val ? chiCStringDup(val) : 0;
}
#endif

int __xpg_strerror_r(int, char*, size_t);

bool chiErrnoString(char* buf, size_t bufsiz) {
#ifdef __GLIBC__
    return !__xpg_strerror_r(errno, buf, bufsiz);
#else
    return !strerror_r(errno, buf, bufsiz);
#endif
}
