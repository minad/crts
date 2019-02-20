enum {
    AUDIT_ARCH        = 0xc000003e,
    SYS_read          = 0,
    SYS_write         = 1,
    SYS_close         = 3,
    SYS_lseek         = 8,
    SYS_mmap          = 9,
    SYS_ioctl         = 16,
    SYS_pread64       = 17,
    SYS_pwrite64      = 18,
    SYS_fcntl         = 72,
    SYS_fdatasync     = 75,
    SYS_personality   = 135,
    SYS_prctl         = 157,
    SYS_clock_gettime = 228,
    SYS_exit_group    = 231,
    SYS_openat        = 257,
    SYS_ppoll         = 271,
    SYS_fallocate     = 285,
};

#define _syscall(...)                                     \
    long r;                                               \
    __asm__ __volatile__ ("syscall"                       \
                          : "=a"(r)                       \
                          : "a"(n), __VA_ARGS__           \
                          : "rcx", "r11", "memory");      \
    return r;

static long syscall1(long n, long a) {
    _syscall("D"(a));
}

static long syscall2(long n, long a, long b) {
    _syscall("D"(a), "S"(b));
}

static long syscall3(long n, long a, long b, long c) {
    _syscall("D"(a), "S"(b), "d"(c));
}

static long syscall4(long n, long a, long b, long c, long d) {
    register long r10 __asm__("r10") = d;
    _syscall("D"(a), "S"(b), "d"(c), "r"(r10));
}

static long syscall5(long n, long a, long b, long c, long d, long e) {
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    _syscall("D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8));
}

static long syscall6(long n, long a, long b, long c, long d, long e, long f) {
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    _syscall("D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9));
}

#undef _syscall
