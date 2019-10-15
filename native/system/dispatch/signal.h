#define NOTIFY_SIG SIGUSR2
static uint32_t sigCount = 0;
static int sigQueue[32];

static void dispatcherNotify(void) {
    pthread_kill(CHI_UN(Task, dispatcherTask), NOTIFY_SIG);
}

static void sigEnqueue(int sig) {
    if (sig != NOTIFY_SIG) {
        sigQueue[sigCount] = sig;
        sigCount = (sigCount + 1) % CHI_DIM(sigQueue);
    }
}

static void sigInstall(int sig) {
    struct sigaction sa = { .sa_handler = sigEnqueue };
    sigfillset(&sa.sa_mask);
    if (sigaction(sig, &sa, 0))
        chiSysErr("sigaction failed");
}

CHI_TASK(dispatcherRun, CHI_UNUSED(arg)) {
    CHI_ASSERT_ONCE;

    sigInstall(SIGINT);
    sigInstall(SIGQUIT);
    sigInstall(SIGUSR1);
    sigInstall(NOTIFY_SIG);

    sigset_t handledSignals;
    sigemptyset(&handledSignals);
    sigaddset(&handledSignals, SIGINT);
    sigaddset(&handledSignals, SIGQUIT);
    sigaddset(&handledSignals, SIGUSR1);
    sigaddset(&handledSignals, NOTIFY_SIG);

    chiTaskName("dispatch");

    for (;;) {
        ChiMicros timeout = dispatchTimers();
        setSigMask(SIG_BLOCK, &handledSignals);
        for (uint32_t i = 0; i < sigCount; ++i)
            dispatchPosixSig(sigQueue[i]);
        sigCount = 0;
        setSigMask(SIG_UNBLOCK, &handledSignals);
        struct timespec ts = nanosToTimespec(chiMicrosZero(timeout) ?
                                             chiNanos(1000000000UL) : chiMicrosToNanos(timeout));
        nanosleep(&ts, 0);
    }
}
