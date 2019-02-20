#include <sandbox.h>
#include <string.h>
#include <stdbool.h>
#if defined(__x86_64__)
#  include "syscall_x86_64.h"
#elif defined(__aarch64__)
#  include "syscall_aarch64.h"
#else
#  error Only x86_64 and aarch64 are supported
#endif

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t s_name;
    uint32_t s_type;
    uint64_t s_flags;
    uint64_t s_addr;
    uint64_t s_offset;
    uint64_t s_size;
    uint32_t s_link;
    uint32_t s_info;
    uint64_t s_addralign;
    uint64_t s_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

enum {
      // clock_gettime
      CLOCK_REALTIME    = 0,
      CLOCK_MONOTONIC   = 1,

      // mmap flags
      PROT_READ         = 1,
      PROT_WRITE        = 2,
      MAP_PRIVATE       = 0x0002,
      MAP_ANONYMOUS     = 0x0020,
      MAP_NORESERVE     = 0x4000,

      // openat flags
      O_CREAT           = 0100,
      O_TRUNC           = 01000,
      O_WRONLY          = 01,
      O_RDWR            = 02,
      O_NONBLOCK        = 04000,
      AT_FDCWD          = -100,

      // lseek
      SEEK_SET          = 0,
      SEEK_END          = 2,

      // errno
      EAGAIN            = 11,

      // fcntl
      F_SETFL           = 4,

      // ppoll
      POLLIN            = 1,
      POLLOUT           = 4,

      // ioctl
      IFF_TAP           = 0x0002,
      IFF_NO_PI         = 0x1000,
      TUNSETIFF         = 0x400454ca,

      // fallocate
      FALLOC_FL_KEEP_SIZE = 1,
      FALLOC_FL_PUNCH_HOLE = 2,
      FALLOC_MODE = FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE,

      // prctl
      PR_SET_NO_NEW_PRIVS = 38,
      PR_SET_SECCOMP      = 22,
      SECCOMP_MODE_FILTER = 2,

      // vdso elf header
      AT_SYSINFO_EHDR	  = 33,
      AT_EXECFN	          = 31,

      // personality
      READ_IMPLIES_EXEC   = 0x0400000,

      // custom config
      MAX_DEVS    = 8,
      BLOCK_UNIT  = 512,
};

// bpf parametrization
#define BPF_BLOCK_FD(i)     (first_block_fd + i)
#define BPF_BLOCK_HI(i)     ((uint32_t)(info.block[i].size >> 32))
#define BPF_BLOCK_LO(i)     ((uint32_t)(info.block[i].size & 0xFFFFFFFF))
#define BPF_BLOCK_MASK      (BLOCK_UNIT - 1)
#define BPF_PPOLL_COUNT     (info.stream_count + info.net_count)
#define BPF_BLOCK_MIN       first_block_fd
#define BPF_BLOCK_MAX       (first_block_fd + info.block_count)

struct sys_timespec {
    long tv_sec;
    long tv_nsec;
};

struct sys_pollfd {
    int   fd;
    short events;
    short revents;
};

struct sys_ifreq {
    char ifr_name[16];
    short int ifr_flags;
    char _ifr_pad[22];
};

struct sys_bpf_insn {
    uint16_t code;
    uint8_t  jt;
    uint8_t  jf;
    uint32_t k;
};

struct sys_bpf_prog {
    uint16_t len;
    struct sys_bpf_insn *insn;
};

struct sb_opts_stream {
    const char* file;
    uint32_t mode;
};

struct sb_opts_net {
    const char* dev;
    uint8_t mac[6];
};

struct sb_opts {
    uint64_t heap;
    struct sb_opts_net net[MAX_DEVS];
    struct sb_opts_stream stream[MAX_DEVS - 2];
    const char* block[MAX_DEVS];
    uint32_t net_count, block_count, stream_count;
    bool unsafe;
};

typedef int (*sys_clock_gettime_t)(int, struct sys_timespec*);

static struct sb_net _net[MAX_DEVS];
static struct sb_block _block[MAX_DEVS];
static struct sb_stream _stream[MAX_DEVS] = { { SB_MODE_READ }, { SB_MODE_WRITE }, { SB_MODE_WRITE } };
static struct sb info = { .net = _net, .block = _block, .stream = _stream };
const struct sb* sb_info = &info;
static uint32_t first_net_fd, first_block_fd,
    read_fd_mask = 1U << SB_STREAM_IN,
    write_fd_mask = (1U << SB_STREAM_OUT) | (1U << SB_STREAM_ERR);
static sys_clock_gettime_t sys_clock_gettime;

static long syscall_result(long r) {
    return r > -4096 && r < 0 ? -1 : r;
}

static long sys_write_errorcode(int fd, const void* buf, size_t size) {
    return syscall3(SYS_write, fd, (long)buf, (long)size);
}

static long sys_write(int fd, const void* buf, size_t size) {
    return syscall_result(sys_write_errorcode(fd, buf, size));
}

static int sys_fcntl(int fd, int cmd, int flags) {
    return (int)syscall_result(syscall3(SYS_fcntl, fd, cmd, flags));
}

static long sys_read_errorcode(int fd, void* buf, size_t size) {
    return syscall3(SYS_read, fd, (long)buf, (long)size);
}

static long sys_pread64(int fd, void* buf, size_t size, uint64_t off) {
    return syscall_result(syscall4(SYS_pread64, fd, (long)buf, (long)size, (long)off));
}

static long sys_pwrite64(int fd, const void* buf, size_t size, uint64_t off) {
    return syscall_result(syscall4(SYS_pwrite64, fd, (long)buf, (long)size, (long)off));
}

static long sys_lseek(int fd, uint64_t off, int whence) {
    return syscall_result(syscall3(SYS_lseek, fd, (long)off, whence));
}

static int sys_personality(unsigned long persona) {
    return (int)syscall_result(syscall1(SYS_personality, (long)persona));
}

static _Noreturn void sys_exit_group(int status) {
    syscall1(SYS_exit_group, status);
    __builtin_unreachable();
}

static int sys_fdatasync(int fd) {
    return (int)syscall_result(syscall1(SYS_fdatasync, fd));
}

static int sys_open(const char* file, int flags, int mode) {
    return (int)syscall_result(syscall4(SYS_openat, AT_FDCWD, (long)file, flags, mode));
}

static int sys_close(int fd) {
    return (int)syscall_result(syscall1(SYS_close, fd));
}

static int sys_clock_gettime_no_vdso(int clock, struct sys_timespec* ts) {
    return (int)syscall_result(syscall2(SYS_clock_gettime, clock, (long)ts));
}

static int sys_fallocate(int fd, int mode, uint64_t off, uint64_t len) {
    return (int)syscall_result(syscall4(SYS_fallocate, fd, mode, (long)off, (long)len));
}

static long sys_mmap(size_t size, int prot, int flags) {
    return syscall_result(syscall6(SYS_mmap, 0, (long)size, prot, flags, -1, 0));
}

static int sys_ioctl(int fd, int req, void* arg) {
    return (int)syscall_result(syscall3(SYS_ioctl, fd, req, (long)arg));
}

static int sys_ppoll(struct sys_pollfd* fd, long nfd, const struct sys_timespec* ts) {
    return (int)syscall_result(syscall5(SYS_ppoll, (long)fd, nfd, (long)ts, 0, 0));
}

static int sys_prctl(int op, long a1, long a2, long a3, long a4) {
    return (int)syscall_result(syscall5(SYS_prctl, op, a1, a2, a3, a4));
}

void sb_exit(int status) {
    sys_exit_group(status);
}

void sb_yield(uint64_t ns, struct sb_mask* mask) {
    struct sys_timespec ts = { .tv_sec = (long)(ns / 1000000000ULL), .tv_nsec = (long)(ns % 1000000000ULL) };
    struct sys_pollfd fd[2 * MAX_DEVS], *fdp = fd;
    for (uint32_t i = 0; i < info.stream_count; ++i)
        *fdp++ = (struct sys_pollfd){ .fd = (int)i,
                                      .events =
                                      (short)((mask->stream_read  & (1U << i) ? POLLIN : 0) |
                                              (mask->stream_write & (1U << i) ? POLLOUT : 0)) };
    for (uint32_t i = 0; i < info.net_count; ++i)
        *fdp++ = (struct sys_pollfd){ .fd = (int)(first_net_fd + i),
                                      .events = (short)(mask->net_read & (1U << i) ? POLLIN : 0) };
    int res = sys_ppoll(fd, (long)(info.stream_count + info.net_count), &ts);
    if (res < 0)
        sb_die("ppoll failed");
    mask->stream_read = mask->stream_write = mask->net_read = 0;
    fdp = fd;
    for (uint32_t i = 0; i < info.net_count; ++i) {
        if (fdp->revents & POLLIN)
            mask->net_read |= 1U << i;
    }
    for (uint32_t i = 0; i < info.stream_count; ++i) {
        if (fdp->revents & POLLIN)
            mask->stream_read |= 1U << i;
        if (fdp->revents & POLLOUT)
            mask->stream_write |= 1U << i;
    }
}

static size_t check_eagain(long res, const char* fail) {
    if (res > -4096 && res < 0) {
        if (res == -EAGAIN)
            return 0;
        sb_die(fail);
    }
    return (size_t)res;
}

size_t sb_stream_write(uint32_t id, const void *buf, size_t size) {
    return check_eagain(sys_write_errorcode((int)id, buf, size), "stream write failed");
}

size_t sb_stream_read(uint32_t id, void *buf, size_t size) {
    return check_eagain(sys_read_errorcode((int)id, buf, size), "stream read failed");
}

static uint64_t get_clock(int clock) {
    struct sys_timespec ts;
    if (sys_clock_gettime(clock, &ts))
        sb_die("clock_gettime failed");
    return 1000000000ULL * (uint64_t)ts.tv_sec + (uint64_t)ts.tv_nsec;
}

uint64_t sb_clock_monotonic(void) {
    return get_clock(CLOCK_MONOTONIC);
}

uint64_t sb_clock_wall(void) {
    return get_clock(CLOCK_REALTIME);
}

void sb_net_write(uint32_t id, const void *buf, size_t size) {
    if (sys_write((int)(first_net_fd + id), buf, size) != (long)size)
        sb_die("write net failed");
}

size_t sb_net_read(uint32_t id, void *buf, size_t size) {
    return check_eagain(sys_read_errorcode((int)(first_net_fd + id), buf, size), "read net failed");
}

void sb_block_write(uint32_t id, uint64_t off, const void *buf, size_t size) {
    if (sys_pwrite64((int)(first_block_fd + id), buf, size, off) != (long)size)
        sb_die("pwrite64 block failed");
}

void sb_block_read(uint32_t id, uint64_t off, void *buf, size_t size) {
    if (sys_pread64((int)(first_block_fd + id), buf, size, off) != (long)size)
        sb_die("pread64 block failed");
}

void sb_block_discard(uint32_t id, uint64_t begin, uint64_t end) {
    if (sys_fallocate((int)(first_block_fd + id), FALLOC_MODE, begin, end - begin))
        sb_die("fallocate block failed");
}

void sb_block_sync(uint32_t id) {
    if (sys_fdatasync((int)(first_block_fd + id)))
        sb_die("fdatasync block failed");
}

static void init_heap(struct sb_heap* heap, size_t size) {
    size *= 1024 * 1024;
    long start = sys_mmap(size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE);
    if (start <= 0)
        sb_die("mmap heap failed");
    heap->start = (uintptr_t)start;
    heap->size = size;
}

static void open_block(int fd, struct sb_block* block, const char* file) {
    sys_close(fd);
    int got = sys_open(file, O_RDWR, 0600);
    if (got < 0)
        sb_die("block image not found");
    if (got != fd)
        sb_die("block descriptor invalid");
    long size = sys_lseek(fd, 0, SEEK_END);
    if (size < 0)
        sb_die("lseek block failed");
    if ((size_t)size < BLOCK_UNIT || (size & (BLOCK_UNIT - 1)))
        sb_die("invalid block image size");
    block->unit = BLOCK_UNIT;
    block->size = (size_t)size;
}

static void set_nonblock(int fd) {
    if (sys_fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
        sb_die("fcntl nonblockfailed");
}

static void open_net(int fd, struct sb_net* net, struct sb_opts_net* opt) {
    struct sys_ifreq ifr = { .ifr_flags = IFF_TAP | IFF_NO_PI };
    if (strlen(opt->dev) + 1 > sizeof (ifr.ifr_name))
        sb_die("net device is invalid");

    sys_close(fd);
    int got = sys_open("/dev/net/tun", O_RDWR | O_NONBLOCK, 0);
    if (got < 0)
        sb_die("open net failed");
    if (got != fd)
        sb_die("net descriptor invalid");

    if (sys_ioctl(fd, TUNSETIFF, &ifr) < 0)
        sb_die("ioctl net failed");

    if (strncmp(ifr.ifr_name, opt->dev, sizeof (ifr.ifr_name)))
        sb_die("net device is invalid");

    set_nonblock(fd);
    memcpy(net->mac, opt->mac, sizeof (net->mac));
    net->mtu = 1500;

    read_fd_mask |= 1U << fd;
    write_fd_mask |= 1U << fd;
}

static void open_stream(int fd, struct sb_stream* stream, struct sb_opts_stream* opt) {
    sys_close(fd);
    int mode =
        (opt->mode & SB_MODE_RW) ? O_RDWR :
        (opt->mode & SB_MODE_WRITE) ? O_WRONLY : 0;
    int got = sys_open(opt->file, O_NONBLOCK | O_CREAT | O_TRUNC | mode, 0600);
    if (got < 0)
        sb_die("open stream file failed");
    if (got != fd)
        sb_die("stream descriptor invalid");
    if (opt->mode & SB_MODE_READ)
        read_fd_mask |= 1U << fd;
    if (opt->mode & SB_MODE_WRITE)
        write_fd_mask |= 1U << fd;
    stream->mode = opt->mode;
}

static void init_seccomp(void) {
    struct sys_bpf_insn insn[] = {
#include "seccomp_filter.h"
    };
    struct sys_bpf_prog prog =
        { .len = sizeof(insn) / sizeof(insn[0]), .insn = insn };
    if (sys_prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)
        || sys_prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, (long)&prog, 0, 0))
        sb_die("prctl failed");
}

static int toxdigit(int c) {
    if (c >= 'A' && c <= 'F')
        return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
        return (c - 'a') + 10;
    if (c >= '0' && c <= '9')
        return c - '0';
    return -1;
}

static size_t parse_size(const char* s) {
    size_t n = 0;
    while (*s >= '0' && *s <= '9')
        n = 10U * n + (size_t)(*s++ - '0');
    return *s ? 0 : n;
}

static bool parse_mac(const char* s, uint8_t* mac) {
    for (uint32_t i = 0; i < 6; ++i) {
        int x = toxdigit(s[3*i]), y = toxdigit(s[3*i+1]);
        mac[i] = (uint8_t)(16 * x + y);
        if (x < 0 || y < 0 || (i < 5 && s[3*i+2] != ':'))
            return false;
    }
    return true;
}

static void parse_arg(struct sb_opts* opts, const char* arg) {
    if (!strcmp(arg, "help")) {
        sb_stream_puts(SB_STREAM_ERR,
                       "Options:\n"
                       "  -Shelp               Show help\n"
                       "  -Sunsafe             Unsafe mode, disable seccomp sandbox\n"
                       "  -Sheap=<size>        Heap size in MiB\n"
                       "  -Sblock=<file>       Block image, multiple supported\n"
                       "  -Sstream=<mode:file> Stream file or FIFO, multiple supported\n"
                       "                       Mode: r, w, rw\n"
                       "  -Snet=<mac@name>     Network device, multiple supported\n"
                       "                       Example: -Snet=01:02:03:04:05:06@name\n");
        sb_exit(0);
    } else if (!strcmp(arg, "unsafe")) {
        opts->unsafe = true;
    } else if (!strncmp(arg, "net=", 4) && arg[4]) {
        if (opts->net_count >= MAX_DEVS)
            sb_die("too many net devices");
        arg += 4;
        char* p = strchr(arg, '@');
        if (strlen(arg) < 19 || !p || !parse_mac(arg, opts->net[opts->net_count].mac))
            sb_die("invalid net specification");
        opts->net[opts->net_count++].dev = p + 1;
    } else if (!strncmp(arg, "block=", 6) && arg[6]) {
        if (opts->block_count >= MAX_DEVS)
            sb_die("too many block devices");
        opts->block[opts->block_count++] = arg + 6;
    } else if (!strncmp(arg, "stream=", 7) && arg[7]) {
        if (opts->stream_count >= MAX_DEVS - 2)
            sb_die("too many stream devices");
        arg += 7;
        char* p = strchr(arg, ':');
        if (!p || !p[1] || p - arg < 1 || p - arg > 2
            || (arg[0] != 'r' && arg[0] != 'w')
            || (arg[1] != 'r' && arg[1] != 'w' && arg[1] != ':'))
            sb_die("invalid stream mode");
        opts->stream[opts->stream_count].mode =
            (arg[0] == 'r' || arg[1] == 'r' ? SB_MODE_READ  : 0U) |
            (arg[0] == 'w' || arg[1] == 'w' ? SB_MODE_WRITE : 0U);
        opts->stream[opts->stream_count++].file = p + 1;
    } else if (!strncmp(arg, "heap=", 5) && arg[5]) {
        opts->heap = parse_size(arg + 5);
    } else {
        sb_stream_puts(SB_STREAM_ERR, "sandbox: Invalid argument '-S");
        sb_stream_puts(SB_STREAM_ERR, arg);
        sb_stream_puts(SB_STREAM_ERR, "'. Try -Shelp.\n");
        sb_exit(1);
    }
}

static void parse_args(struct sb_opts* opts, int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        if (!strncmp(argv[i], "-S", 2))
            parse_arg(opts, argv[i] + 2);
    }
}

static void clean_args(int *argc, char** argv) {
    char* p = strrchr(argv[0], '/');
    if (p) {
        ++p;
        size_t n = strlen(argv[0]), d = (size_t)(p - argv[0]);
        memmove(argv[0], p, n - d);
        memset(argv[0] + n - d, 0, d);
    }
    int n = 1;
    for (int i = 1; i < *argc; ++i) {
        if (strncmp(argv[i], "-S", 2)) {
            argv[n++] = argv[i];
        } else {
            memset(argv[i], 0, strlen(argv[i]));
            argv[i] = 0;
        }
    }
    *argc = n;
    argv[*argc] = 0;
}

static void clean_env(char** env) {
    while (*env) {
        memset(*env, 0, strlen(*env));
        *env++ = 0;
    }
}

static void clean_auxv(long* auxv, void** ehdr) {
    for (; *auxv; auxv += 2) {
        if (auxv[0] == AT_EXECFN)
            memset((char*)auxv[1], 0, strlen((char*)auxv[1]));
        else if (auxv[0] == AT_SYSINFO_EHDR)
            *ehdr = (void*)auxv[1];
        auxv[0] = 0;
        auxv[1] = 0;
    }
}

static void* vdsosym(void* start, const char* fn) {
    if (!start)
        return 0;
    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)start;
    const Elf64_Shdr* sh = (const Elf64_Shdr*)(void*)((char*)start + eh->e_shoff);

    const char* dynstr = 0;
    const Elf64_Shdr* dynsym = 0;
    for (size_t i = 0; (!dynstr || !dynsym) && i < eh->e_shnum; ++i) {
        const char* name = (char*)start + sh[eh->e_shstrndx].s_offset + sh[i].s_name;
        if (!strcmp(name, ".dynstr"))
            dynstr = (char*)start + sh[i].s_offset;
        else if (!strcmp(name, ".dynsym"))
            dynsym = sh + i;
    }

    for (size_t i = 0; dynsym && dynstr && i < dynsym->s_size / dynsym->s_entsize; ++i) {
        const Elf64_Sym* sym = (const Elf64_Sym*)(void*)((char*)start + dynsym->s_offset) + i;
        if (!strcmp(dynstr + sym->st_name, fn))
            return (char*)start + sym->st_value;
    }
    return 0;
}

static void init_vdso(void* ehdr) {
    sys_clock_gettime = (sys_clock_gettime_t)vdsosym(ehdr, "clock_gettime");
    if (!sys_clock_gettime) {
        sb_warn("clock_gettime not found in vdso");
        sys_clock_gettime = sys_clock_gettime_no_vdso;
    }
}

static void check_persona(void) {
    int persona = sys_personality(0xffffffff);
    if (persona < 0)
        sb_die("personality failed");
    if (persona & READ_IMPLIES_EXEC)
        sb_die("READ_IMPLIES_EXEC is unacceptable");
}

void _sb_start(int, char**);
void _sb_start(int argc, char** argv) {
    char** env = argv + argc + 1, **auxv = env;
    while (*auxv++);
    check_persona();

    void* ehdr = 0;
    clean_auxv((long*)auxv, &ehdr);
    init_vdso(ehdr);

    struct sb_opts opts = { .heap = 128 };
    parse_args(&opts, argc, argv);
    init_heap(&info.heap, opts.heap);

    set_nonblock(SB_STREAM_IN);
    info.stream_count = opts.stream_count + SB_STREAM_AUX;
    info.block_count = opts.block_count;
    info.net_count = opts.net_count;
    uint32_t fd = SB_STREAM_AUX;
    for (uint32_t i = 0; i < opts.stream_count; ++i)
        open_stream((int)fd++, info.stream + SB_STREAM_AUX + i, opts.stream + i);
    first_net_fd = fd;
    for (uint32_t i = 0; i < opts.net_count; ++i)
        open_net((int)fd++, info.net + i, opts.net + i);
    first_block_fd = fd;
    for (uint32_t i = 0; i < opts.block_count; ++i)
        open_block((int)fd++, info.block + i, opts.block[i]);

    // clean the environment.
    // it would probably be better recreate a new pristine stack.
    // besides that, it is not clear to me if there is other
    // data in the address space somewhere, which would be leaked.
    clean_args(&argc, argv);
    clean_env(env);

    if (opts.unsafe)
        sb_warn("unsafe mode, seccomp sandbox disabled!");
    else
        init_seccomp();

    sb_exit(sb_main(argc, argv));
}

#if defined(__x86_64__)
__asm__ (".globl _start\n"
         "_start:\n"
         "mov %rsp, %rbp\n"    // initial frame
         "mov 0(%rbp), %rdi\n" // argc
         "lea 8(%rbp), %rsi\n" // argv
         "call _sb_start\n");
#elif defined(__aarch64__)
__asm__ (".globl _start\n"
         "_start:\n"
         "ldr x0, [sp, #0]\n" // argc
         "add x1, sp, #8\n"   // argv
         "bl _sb_start\n");
#endif
