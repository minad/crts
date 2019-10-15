#include <sys/event.h>

static int notifyFd = 0;

static void dispatcherNotify(void) {
    uint64_t inc = 1;
    if (notifyFd) {
        if (write(notifyFd, &inc, sizeof (inc)) < 0)
            chiSysErr("write");
    }
}

CHI_TASK(dispatcherRun, CHI_UNUSED(arg)) {
    CHI_ASSERT_ONCE;

    int kq = kqueue();
    if (kq < 0)
        chiSysErr("kqueue");

    int pipefd[2];
    if (pipe(pipefd))
        chiSysErr("pipe");
    notifyFd = pipefd[1];

    chiTaskName("dispatch");

    enum { MAXEVENTS = 4 };

    struct kevent evSet;
    static const int sig[] = { SIGQUIT, SIGINT };
    for (size_t i = 0; i < CHI_DIM(sig); ++i) {
        EV_SET(&evSet, (uintptr_t)sig[i], EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
        if (kevent(kq, &evSet, 1, 0, 0, 0))
	    chiSysErr("kevent");
    }

    EV_SET(&evSet, (uintptr_t)pipefd[0], EVFILT_READ, EV_ADD, 0, 0, 0);
    if (kevent(kq, &evSet, 1, 0, 0, 0))
        chiSysErr("kevent");

    for (;;) {
        ChiMicros timeout = dispatchTimers();
        struct timespec ts = nanosToTimespec(chiMicrosToNanos(timeout));
        struct kevent ev[MAXEVENTS];
        int nevents = kevent(kq, 0, 0, ev, MAXEVENTS, chiMicrosZero(timeout) ? 0 : &ts);
        if (nevents < 0) {
            if (errno == EINTR)
                continue;
            chiSysErr("kevent");
        }
        for (int i = 0; i < nevents; ++i) {
            if (ev[i].filter == EVFILT_SIGNAL) {
                dispatchPosixSig((int)ev[i].ident);
            } else {
                uint64_t x;
                if (read(pipefd[0], &x, sizeof (uint64_t)) < 0)
                    chiErr("Reading from notify fd failed: %m");
            }
	}
    }
}
