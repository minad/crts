#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

static int notifyFd = 0;

static void dispatcherNotify(void) {
    uint64_t inc = 1;
    if (notifyFd) {
        if (write(notifyFd, &inc, sizeof (inc)) < 0)
            chiSysErr("write");
    }
}

static void dispatcherRun(void* CHI_UNUSED(arg)) {
    sigset_t handledSignals;
    sigemptyset(&handledSignals);
    sigaddset(&handledSignals, SIGINT);
    sigaddset(&handledSignals, SIGQUIT);
    sigaddset(&handledSignals, SIGUSR1);

    int sigfd = signalfd(-1, &handledSignals, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sigfd < 0)
        chiSysErr("signalfn");

    int notify = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (notify < 0)
        chiSysErr("eventfd");
    notifyFd = notify;

    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
        chiSysErr("timerfd_create");

    int efd = epoll_create1(EPOLL_CLOEXEC);
    if (efd < 0)
        chiSysErr("epoll_create1");

    chiTaskName("dispatch");

    enum { MAXEVENTS = 4 };
    struct epoll_event ev[MAXEVENTS];
    memset(ev, 0, sizeof (struct epoll_event)); // initializer doesn't work with clang
    ev->events = EPOLLIN;
    ev->data.fd = sigfd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, sigfd, ev))
        chiSysErr("epoll_ctl");

    memset(ev, 0, sizeof (struct epoll_event));
    ev->events = EPOLLIN;
    ev->data.fd = timerfd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, timerfd, ev))
        chiSysErr("epoll_ctl");

    memset(ev, 0, sizeof (struct epoll_event));
    ev->events = EPOLLIN;
    ev->data.fd = notify;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, notify, ev))
        chiSysErr("epoll_ctl");

    for (;;) {
        ChiMicros timeout = dispatchTimers();
        if (!chiMicrosZero(timeout)) {
            struct itimerspec it = { .it_value = nanosToTimespec(chiMicrosToNanos(timeout)) };
            if (timerfd_settime(timerfd, 0, &it, 0))
                chiSysErr("timerfd_settime");
        }

        int nevents = epoll_wait(efd, ev, MAXEVENTS, -1);
        if (nevents < 0) {
            if (errno == EINTR)
                continue;
            chiSysErr("epoll_wait");
        }

        for (int i = 0; i < nevents; ++i) {
            if (ev[i].data.fd == sigfd) {
                struct signalfd_siginfo ssi[MAXEVENTS];
                ssize_t nsigs = read(sigfd, ssi, sizeof (ssi)) / (ssize_t)sizeof (struct signalfd_siginfo);
                for (ssize_t j = 0; j < nsigs; ++j)
                    dispatchPosixSig((int)ssi[j].ssi_signo);
            } else if (ev[i].data.fd == notify || ev[i].data.fd == timerfd) {
                uint64_t x;
                if (read(ev[i].data.fd, &x, sizeof (uint64_t)) != sizeof (uint64_t))
                    chiErr("Reading from event fd failed: %m");
            }
        }
    }
}
