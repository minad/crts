enum {
    AUDIT_ARCH        = 0xc00000b7,
    SYS_fcntl         = 25,
    SYS_ioctl         = 29,
    SYS_fallocate     = 47,
    SYS_openat        = 56,
    SYS_close         = 57,
    SYS_lseek         = 62,
    SYS_read          = 63,
    SYS_write         = 64,
    SYS_pread64       = 67,
    SYS_pwrite64      = 68,
    SYS_ppoll         = 73,
    SYS_fdatasync     = 83,
    SYS_personality   = 92,
    SYS_exit_group    = 94,
    SYS_clock_gettime = 113,
    SYS_prctl         = 167,
    SYS_mmap          = 222,
};

#define _syscall(...)                                                   \
    register long x8 __asm__("x8") = n;                                 \
    __asm__ __volatile__ ("svc 0"                                       \
                          : "=r"(x0)                                    \
                          : "r"(x8), __VA_ARGS__                        \
                          : "memory", "cc");                            \
    return x0;                                                          \

static inline long syscall1(long n, long a) {
    register long x0 __asm__("x0") = a;
    _syscall("0"(x0));
}

static inline long syscall2(long n, long a, long b) {
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    _syscall("0"(x0), "r"(x1));
}

static inline long syscall3(long n, long a, long b, long c) {
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    _syscall("0"(x0), "r"(x1), "r"(x2));
}

static inline long syscall4(long n, long a, long b, long c, long d) {
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    _syscall("0"(x0), "r"(x1), "r"(x2), "r"(x3));
}

static inline long syscall5(long n, long a, long b, long c, long d, long e) {
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    register long x4 __asm__("x4") = e;
    _syscall("0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4));
}

static inline long syscall6(long n, long a, long b, long c, long d, long e, long f) {
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    register long x4 __asm__("x4") = e;
    register long x5 __asm__("x5") = f;
    _syscall("0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5));
}

#undef _syscall
