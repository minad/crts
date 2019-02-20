#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include "../strutil.h"
#include "interrupt.h"
#include "tasklocal.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#  include <pthread_np.h>
#  define pthread_setname_np pthread_set_name_np
#elif defined(__MACH__)
#  define pthread_setname_np(t, n) pthread_setname_np(n)
#elif defined (__linux__)
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

#ifdef MAP_NORESERVE
#  define _CHI_MAP_NORESERVE MAP_NORESERVE
#else
#  define _CHI_MAP_NORESERVE 0
#endif

#if defined(CLOCK_MONOTONIC_COARSE)
#  define _CHI_CLOCK_REAL_FAST CLOCK_MONOTONIC_COARSE
#elif defined(CLOCK_MONOTONIC_FAST)
#  define _CHI_CLOCK_REAL_FAST CLOCK_MONOTONIC_FAST
#else
#  define _CHI_CLOCK_REAL_FAST CLOCK_MONOTONIC
#endif

static pthread_key_t taskLocalKey;
static ChiTask       dispatcherTask;

static struct timespec nanosToTimespec(ChiNanos ns) {
    return (struct timespec) { .tv_sec = (long)(CHI_UN(Nanos, ns) / 1000000000UL),
            .tv_nsec = (long)(CHI_UN(Nanos, ns) % 1000000000UL) };
}

static ChiNanos timespecToNanos(struct timespec tv) {
    return (ChiNanos){(uint64_t)tv.tv_sec * 1000000000 + (uint64_t)tv.tv_nsec};
}

static ChiNanos timevalToNanos(struct timeval tv) {
    return (ChiNanos){(uint64_t)tv.tv_sec * 1000000000 + (uint64_t)tv.tv_usec * 1000};
}

static ChiSig convertPosixSig(int sig) {
    switch (sig) {
    case SIGQUIT: return CHI_SIG_DUMPHEAP;
    case SIGUSR1: return CHI_SIG_DUMPSTACK;
    case SIGINT:  return CHI_SIG_INTERRUPT;
    default:      CHI_BUG("Wrong signal");
    }
}

static void dispatchPosixSig(int sig) {
    if (sig == SIGQUIT || sig == SIGUSR1 || sig == SIGINT)
        dispatchSig(convertPosixSig(sig));
}

static void setSigMask(int how, sigset_t* set) {
    _CHI_PTHREAD_CHECK(pthread_sigmask(how, set, 0));
}

static long cachedSysconf(int c, long* n) {
    if (!*n) {
        *n = sysconf(c);
        if (*n <= 0)
            chiSysErr("sysconf");
    }
    return *n;
}

#ifdef __linux__
#  include "dispatch/epoll.h"
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__MACH__)
#  include "dispatch/kqueue.h"
#else
#  include "dispatch/signal.h"
#endif

void chiSystemSetup(void) {
    _CHI_PTHREAD_CHECK(pthread_key_create(&taskLocalKey, destroyTaskLocal));

    interruptSetup();

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

    struct sigaction sa = { .sa_handler = SIG_IGN };
    if (sigaction(SIGPIPE, &sa, 0))
        chiSysErr("sigaction");

    dispatcherTask = chiTaskCreate(dispatcherRun, 0);
}

static ChiNanos getClock(clockid_t clock) {
    struct timespec ts;
    if (clock_gettime(clock, &ts))
        chiSysErr("clock_gettime");
    return timespecToNanos(ts);
}

static clockid_t clockId(ChiClock c) {
    switch (c) {
    case CHI_CLOCK_REAL_FINE: return CLOCK_MONOTONIC;
    case CHI_CLOCK_REAL_FAST: return _CHI_CLOCK_REAL_FAST;
    case CHI_CLOCK_CPU: return CLOCK_PROCESS_CPUTIME_ID;
    }
    CHI_BUG("Invalid clock id");
}

ChiNanos chiClock(ChiClock c) {
    return getClock(clockId(c));
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

typedef struct {
    ChiTaskRun run;
    void* arg;
} TaskArg;

static void* taskRun(void* arg) {
    TaskArg ta = *(TaskArg*)arg;
    chiFree(arg);
    ta.run(ta.arg);
    return 0;
}

ChiTask chiTaskTryCreate(ChiTaskRun run, void* arg) {
    TaskArg* ta = chiAllocObj(TaskArg);
    ta->run = run;
    ta->arg = arg;
    ChiTask t;
    int ret = pthread_create(CHI_UNP(Task, &t), 0, taskRun, ta);
    if (ret) {
        chiFree(ta);
        errno = ret;
        CHI_CLEAR(&t);
    }
    return t;
}

ChiTask chiTaskCreate(ChiTaskRun run, void* arg) {
    ChiTask t = chiTaskTryCreate(run, arg);
    if (chiTaskNull(t))
        chiSysErr("pthread_create");
    return t;
}

ChiTask chiTaskCurrent(void) {
    return (ChiTask){ pthread_self() };
}

_Noreturn void chiTaskExit(void) {
    pthread_exit(0);
}

void chiTaskYield(void) {
    sched_yield();
}

void chiTaskJoin(ChiTask t) {
    _CHI_PTHREAD_CHECK(pthread_join(CHI_UN(Task, t), 0));
}

void chiTaskCancel(ChiTask t) {
    pthread_kill(CHI_UN(Task, t), SIGPIPE);
}

void chiTaskClose(ChiTask CHI_UNUSED(t)) {
}

bool chiTaskEqual(ChiTask a, ChiTask b) {
    return pthread_equal(CHI_UN(Task, a), CHI_UN(Task, b));
}

bool chiTaskNull(ChiTask a) {
    ChiTask b;
    CHI_CLEAR(&b);
    return chiTaskEqual(a, b);
}

void chiTaskName(const char* name) {
    // don't change name of process to allow killall
    if (!pthread_main_np())
        pthread_setname_np(pthread_self(), name);
}

void* chiTaskLocal(size_t size, ChiDestructor destructor) {
    TaskLocal* oldLocal = (TaskLocal*)pthread_getspecific(taskLocalKey);
    TaskLocal* newLocal = (TaskLocal*)chiAlloc(sizeof (TaskLocal) + size);
    newLocal->destructor = destructor;
    newLocal->next = oldLocal;
    _CHI_PTHREAD_CHECK(pthread_setspecific(taskLocalKey, newLocal));
    return newLocal->data;
}

static void madivse_err(void* p, size_t s, int flags) {
    if (flags && madvise(p, s, flags))
        chiSysErr("madvise");
}

void* chiVirtAlloc(void* ptr, size_t size, int32_t flags) {
    ptr = mmap(ptr, size,
               PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS |
               MAP_PRIVATE |
               (ptr ? MAP_FIXED : 0) |
               ((flags & CHI_MEMMAP_NORESERVE) ? _CHI_MAP_NORESERVE : 0),
               -1, 0);
    if (ptr == MAP_FAILED)
        return 0;
    if (flags & CHI_MEMMAP_HUGE)
        madivse_err(ptr, size, _CHI_MADV_HUGE);
    else if (flags & CHI_MEMMAP_NOHUGE)
        madivse_err(ptr, size, _CHI_MADV_NOHUGE);
    return ptr;
}

void chiVirtFree(void* p, size_t s) {
    if (munmap(p, s))
        chiSysErr("munmap");
}

uint32_t chiPhysProcessors(void) {
    static long nprocessors_onln = 0;
    return (uint32_t)cachedSysconf(_SC_NPROCESSORS_ONLN, &nprocessors_onln);
}

uint64_t chiPhysMemory(void) {
    static long phys_pages = 0, pagesize = 0;
    return ((uint64_t)cachedSysconf(_SC_PHYS_PAGES, &phys_pages) *
            (uint64_t)cachedSysconf(_SC_PAGESIZE, &pagesize)) / CHI_MiB(1) * CHI_MiB(1);
}

bool chiFilePerm(const char* file, int32_t perm) {
    int flags = F_OK;
    if (perm & CHI_FILE_READABLE)
        flags |= R_OK;
    if (perm & CHI_FILE_EXECUTABLE)
        flags |= X_OK;
    return !access(file, flags);
}

void chiActivity(ChiActivity* a) {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    a->cpuTimeUser = timevalToNanos(r.ru_utime);
    a->cpuTimeSystem = timevalToNanos(r.ru_stime);
    a->residentSize = (size_t)r.ru_maxrss * 1024;
    a->pageFault = (uint64_t)r.ru_minflt;
    a->pageSwap = (uint64_t)r.ru_majflt;
    a->contextSwitch = (uint64_t)r.ru_nivcsw;
    a->diskRead = (uint64_t)r.ru_inblock;
    a->diskWrite = (uint64_t)r.ru_oublock;
}

#if CHI_PAGER_ENABLED
void chiPager(void) {
    if (!chiTerminal(STDOUT_FILENO))
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
        dup2(fd[0], 0);
        close(fd[0]);
        close(fd[1]);
        execl("/bin/sh", "sh", "-c", pager, NULL);
        chiSysErr("execl");
    }
    // child
    dup2(fd[1], 1);
    close(fd[0]);
    close(fd[1]);
}
#endif

uint32_t chiPid(void) {
    return (uint32_t)getpid();
}

bool chiTerminal(int fd) {
    const char* term = CHI_ENV_ENABLED ? getenv("TERM") : 0;
    if (term && streq(term, "dumb"))
        return false;
    // Cache result such that it works if the pager is used
    static int termOut = 0;
    if (!termOut)
        termOut = isatty(STDOUT_FILENO) ? 1 : -1;
    return fd == STDOUT_FILENO ? termOut > 0 : !!isatty(fd);
}

FILE* chiFileOpenWrite(const char* file) {
    FILE* fp = fopen(file, "ae");
    if (!fp)
        return 0;
    off_t off = ftello(fp);
    if (off) {
        int err = off > 0 ? EEXIST : errno;
        fclose(fp);
        errno = err;
        return 0;
    }
    return fp;
}

FILE* chiFileOpenRead(const char* name) {
    return fopen(name, "re");
}

FILE* chiOpenFd(int fd) {
    fd = dup(fd);
    return fd >= 0 ? fdopen(fd, "we") : 0;
}

FILE* chiOpenPipe(const char* cmd) {
    FILE* fp = popen(cmd, "we");
#ifdef F_SETPIPE_SZ
    if (fp && fcntl(fileno(fp), F_SETPIPE_SZ, CHI_MiB(1)) < 0) {
        pclose(fp);
        fp = 0;
    }
#endif
    return fp;
}

#if CHI_ENV_ENABLED
char* chiGetEnv(const char* var) {
    const char* val = getenv(var);
    return val ? chiCStringDup(val) : 0;
}
#endif

int __xpg_strerror_r(int, char*, size_t);

bool chiErrorString(char* buf, size_t bufsiz) {
#ifdef __GLIBC__
    return !__xpg_strerror_r(errno, buf, bufsiz);
#else
    return !strerror_r(errno, buf, bufsiz);
#endif
}
