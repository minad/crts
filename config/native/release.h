/*
 * Configuration file for the native Chili runtime.
 */

#include "compiler.h"

#if defined(CHI_STANDALONE_SANDBOX) || defined(CHI_STANDALONE_WASM)
#  define _CHI_STANDALONE
#endif

#if defined(_CHI_STANDALONE)
#  define CHI_SYSTEM_HAS_STDIO            0
#  define CHI_SYSTEM_HAS_ERRNO            0
#  define CHI_SYSTEM_HAS_MALLOC           0
#  define CHI_SYSTEM_HAS_INTERRUPT        0
#  define CHI_SYSTEM_HAS_TASKS            0
#  define CHI_SYSTEM_HAS_PARTIAL_VIRTFREE 1
#elif defined(_WIN32)
#  define CHI_SYSTEM_HAS_STDIO            1
#  define CHI_SYSTEM_HAS_ERRNO            1
#  define CHI_SYSTEM_HAS_MALLOC           1
#  define CHI_SYSTEM_HAS_INTERRUPT        1
#  define CHI_SYSTEM_HAS_TASKS            1
#  define CHI_SYSTEM_HAS_PARTIAL_VIRTFREE 0
#else
#  define CHI_SYSTEM_HAS_STDIO            1
#  define CHI_SYSTEM_HAS_ERRNO            1
#  define CHI_SYSTEM_HAS_MALLOC           1
#  define CHI_SYSTEM_HAS_INTERRUPT        1
#  define CHI_SYSTEM_HAS_TASKS            1
#  define CHI_SYSTEM_HAS_PARTIAL_VIRTFREE 1
#endif

/**
 * Inlining aggressiveness
 */
/*#define CHI_INL static __attribute__ ((unused))*/
#define CHI_INL static inline
/*#define CHI_INL static inline __attribute__((always_inline))*/

/**
 * Maximum number of arguments of a continuation.
 * Continuations with more arguments are split into
 * curried functions as of now. An alternative
 * would be to put the additional arguments on the stack.
 */
#define CHI_AMAX                   16

/**
 * Enable NaN boxing. Reference types are stored
 * within 48 bits, which are available in NaN values.
 * The big advantage is that 64-bit double values can
 * be stored unboxed.
 * However since only 48 bits are available for reference
 * types, there is not much space for metadata.
 *
 * Within the metadata we store the size, the type
 * and the address in the heap. The consequence is
 * that only heap sizes up to 256G are supported.
 *
 * Another serious issue of this scheme is that
 * the compressed pointers point to a *fixed region*
 * in the virtual memory address space, the chunk arena.
 * Obviously this partially defeats ASLR.
 */
#ifdef CHI_STANDALONE_SANDBOX
#  define CHI_NANBOXING_ENABLED      0
#else
#  define CHI_NANBOXING_ENABLED      1
#endif

/**
 * Minimum block size in words
 */
#define CHI_BLOCK_MINSIZE          512 /* 4K */

/**
 * Maximum block size in words
 */
#define CHI_BLOCK_MAXSIZE          131072 /* 1M */

/**
 * Maximum number of generations in the minor heap.
 * The number can be chosen via the command line.
 */
#define CHI_GEN_MAX                10

/**
 * Enable location information for stack traces
 */
#define CHI_LOC_ENABLED            1

/**
 * Offset of continuation info variable next to function
 */
#ifdef _WIN32
#  define CHI_CONTINFO_ALIGN         32
#  define CHI_CONTINFO_OFFSET        32
#else
#  define CHI_CONTINFO_ALIGN         16
#  define CHI_CONTINFO_OFFSET        16
#endif

#ifdef CHI_STANDALONE_WASM
#  define CHI_CONTINFO_PREFIX        0
#else
#  define CHI_CONTINFO_PREFIX        1
#endif

/**
 * Enable tracepoints in native code for enhanced profiling.
 * Without tracepoints native code won't appear in the profiler output!
 */
#define CHI_TRACEPOINTS_ENABLED    0

/**
 * Enable the command line parser for the runtime options.
 */
#define CHI_OPTION_ENABLED         1

/**
 * Enable colors
 */
#define CHI_COLOR_ENABLED          1

/* -------------------- Event system settings --------------------  */

#define CHI_STATS_ENABLED          1        /** Enable the statistics system */
#define CHI_STATS_SCAV_ENABLED     1        /** Enable scavenger statistics */
#define CHI_STATS_MARK_ENABLED     1        /** Enable marking statistics */
#define CHI_STATS_SWEEP_ENABLED    1        /** Enable sweeper scanner */
#define CHI_EVENT_ENABLED          1        /** Enable the event system */
#define CHI_EVENT_XML_ENABLED      1        /** Enable the xml log writer */
#define CHI_EVENT_CSV_ENABLED      1        /** Enable the csv log writer */
#define CHI_EVENT_TE_ENABLED       1        /** Enable the trace event log writer, format consumed by chromium */
#define CHI_EVENT_JSON_ENABLED     1        /** Enable the json log writer */
#define CHI_EVENT_PRETTY_ENABLED   1        /** Enable the pretty log writer */
#define CHI_EVENT_MP_ENABLED       1        /** Enable the messagpack log writer */
#define CHI_EVENT_FILTER_SIZE      12       /** Size of the event filter bitmask */

#define CHI_EVENT_TID_RUNTIME      0
#define CHI_EVENT_TID_WORKER       1
#define CHI_EVENT_TID_UNNAMED      1000
#define CHI_EVENT_TID_THREAD       1001

#define CHI_HASH_MAXFILL           80
#define CHI_HASH_INITSHIFT         8 /* size = 2 << shift */
#define CHI_VEC_GROWTH             200
#define CHI_QUICK_SORT_CUTOFF      16

/**
 * Compile with support for heap checking.
 * The heap check is very expensive and must be turned on via the command line.
 */
#define CHI_HEAP_CHECK_ENABLED     1

/**
 * Compile with support for heap dumping.
 */
#define CHI_HEAP_DUMP_ENABLED      1

/**
 * Compile with support for heap profiling.
 */
#define CHI_HEAP_PROF_ENABLED      1

/**
 * Enable DTrace probes
 */
#define CHI_DTRACE_ENABLED         0

/**
 * Enable LTTng trace points
 */
#define CHI_LTTNG_ENABLED          0

/**
 * Enable small string optimization.
 * Small strings are stored unboxed if enabled.
 */
#define CHI_STRING_SMALLOPT        1

/**
 * Enable small integer optimization
 * for big integers. Small integers
 * are stored unboxed if enabled.
 */
#define CHI_BIGINT_SMALLOPT        1

/**
 * Select the big integer backend. Currently
 * tommath and gmp are supported. Changing
 * this setting does not change the binary
 * API of the runtime, since the calls to the
 * respective libraries are wrapped.
 */
#define CHI_BIGINT_BACKEND       tommath

/**
 * Big integer size limit in bytes.
 *
 * The size must be limited since big integer allocations
 * must not fail. For example if the custom malloc fails
 * in GMP, the program is terminated.
 * However libtommath would allow more graceful error handling.
 *
 */
#define CHI_BIGINT_LIMIT           102400 /* 100K */

/**
 * Enable a fixed chunk memory arena.
 * This partially defeats ASLR.
 * On 64 bit platforms, this setting must be enabled for NaN boxing.
 * See CHI_NANBOXING_ENABLED.
 */
#ifdef _CHI_STANDALONE
#  define CHI_CHUNK_ARENA_ENABLED   1
#  define CHI_CHUNK_ARENA_FIXED     0
#elif CHI_ARCH_BITS == 64 && CHI_NANBOXING_ENABLED
#  define CHI_CHUNK_ARENA_ENABLED   1
#  define CHI_CHUNK_ARENA_FIXED     1
#else
#  define CHI_CHUNK_ARENA_ENABLED   0
#  define CHI_CHUNK_ARENA_FIXED     0
#endif

#if CHI_CHUNK_ARENA_FIXED
#  if defined (CHI_ARCH_ARM64)
#    define CHI_CHUNK_MAX_SHIFT     37 /* 128G */
#    define CHI_CHUNK_START         ((uintptr_t)1 << 37) /* start address (bytes) */
#  else
#    define CHI_CHUNK_MAX_SHIFT     38 /* 256G */
#    define CHI_CHUNK_START         ((uintptr_t)1 << 45) /* start address (bytes) */
#  endif
#else
#  if CHI_ARCH_BITS == 32
#    define CHI_CHUNK_MAX_SHIFT     31 /* 2G */
#    define CHI_CHUNK_START         0UL
#  else
#    define CHI_CHUNK_MAX_SHIFT     48 /* 256T */
#    define CHI_CHUNK_START         0UL
#  endif
#endif
#ifdef _WIN32
#  define CHI_CHUNK_MIN_SHIFT     16 /* 64K, windows allocation granularity */
#else
#  define CHI_CHUNK_MIN_SHIFT     12 /* 4K */
#endif
#define CHI_CHUNK_MIN_SIZE        ((uintptr_t)1 << CHI_CHUNK_MIN_SHIFT)
#define CHI_CHUNK_MAX_SIZE        ((uintptr_t)1 << CHI_CHUNK_MAX_SHIFT)
#define CHI_CHUNK_MAX             (CHI_CHUNK_MAX_SHIFT - CHI_CHUNK_MIN_SHIFT)
#define CHI_CHUNK_MEMMAP_TRIES    10

/**
 * Enable the memory unmapper background task.
 * Returning memory to the OS is usually very slow,
 * therefore the task is needed.
 * Memory is not returned immediately but
 * only after a certain timeout.
 */
#ifdef _CHI_STANDALONE
#  define CHI_UNMAP_TASK_ENABLED    0
#else
#  define CHI_UNMAP_TASK_ENABLED    1
#endif

/**
 * Enable the primitive Chili malloc implementation, if system has none.
 */
#ifdef _CHI_STANDALONE
#  define CHI_MALLOC_ENABLED 1
#else
#  define CHI_MALLOC_ENABLED 0
#endif

/**
 * Select the calling convention
 */
#if defined(CHI_ARCH_X86_64) && __has_include("tailcall.h")
#  ifdef __clang__
#    define CHI_CALLCONV           tail_stack_arg8
#  else
#    define CHI_CALLCONV           tail_stack_arg6
#  endif
#elif defined(CHI_STANDALONE_WASM)
/* TODO: If WebAssembly supports tailcalls, we can also use tail_tls */
#  define CHI_CALLCONV             trampoline_tls
#else
#  define CHI_CALLCONV             trampoline_stack
#endif

/**
 * Activate memory poisoning, only in enabled
 * in debugging mode.
 */
#define CHI_POISON_ENABLED         0

/**
 * Expensive poisoning of objects. Objects are not poisoned by
 * default! Enable it for debugging problems with the garbage
 * collector which might result from objects,
 * which are not initialized properly.
 */
#define CHI_POISON_OBJECT_ENABLED  0
#define CHI_POISON_BLOCK_ENABLED   0

/*
 * Values for memory poisoning (Debugging only)
 */

#define CHI_POISON_BLOCK_CANARY    0xB10CB10C
#define CHI_POISON_BLOCK_FREE      0xA5
#define CHI_POISON_BLOCK_USED      0x5A
#define CHI_POISON_MEM_USED        0xB6
#define CHI_POISON_LIST            0x4c495354
#define CHI_POISON_DESTROYED       0xDD
#define CHI_POISON_MEM_BEGIN       0x585843414e415259ULL
#define CHI_POISON_MEM_END         0x5952414e41435858ULL

/*
 * Stack debugging settings
 */

#define CHI_STACK_CANARY           0xDADAD0D0DADAD0D0ULL
#define CHI_STACK_CANARY_SIZE      0
#define CHI_STACK_DEBUG_WALK       0

/*
 * Enable debugging libraries
 */

#define CHI_HEAPTRACK_ENABLED      __has_include(<heaptrack_api.h>)
#define CHI_VALGRIND_ENABLED       __has_include(<valgrind/valgrind.h>) && __has_include(<valgrind/memcheck.h>)

/**
 * Collect statistics for CHI_LIKELY/CHI_UNLIKELY macros,
 * to check if their placement is justified. This option
 * is only useful for debugging.
 */
#define CHI_LIKELY_STATS_ENABLED   0

#define CHI_PROF_ENABLED           0

/**
 * Use polling instead of system timers for timeouts
 */
#ifdef _CHI_STANDALONE
#  define CHI_TIMEOUT_POLLING      1
#else
#  define CHI_TIMEOUT_POLLING      0
#endif

/**
 * Enable paged output for help and disassembler.
 */
#if defined(_WIN32) || defined(_CHI_STANDALONE)
#  define CHI_PAGER_ENABLED          0
#else
#  define CHI_PAGER_ENABLED          1
#endif

/**
 * Use absolute pager path for security.
 * If this needs tweaking overwrite the environment variable.
 */
#define CHI_PAGER                  "/bin/less --LONG-PROMPT --quit-if-one-screen --no-init --raw-control-chars"

/**
 * Enable various special sinks
 */
#ifndef _CHI_STANDALONE
#  define CHI_SINK_ZSTD_ENABLED    1
#else
#  define CHI_SINK_ZSTD_ENABLED    0
#endif

#ifdef _WIN32
#  define CHI_SINK_FD_ENABLED      0
#else
#  define CHI_SINK_FD_ENABLED      1
#endif

/**
 * Use absolute pager path for security.
 * If this needs tweaking overwrite the environment variable.
 */
#ifdef _WIN32
#  define CHI_ZSTD                 "zstd.exe"
#else
#  define CHI_ZSTD                 "/usr/bin/zstd"
#endif

/**
 * Enable accessing environment variables.
 * WARNING: Disable this if the environment cannot be trusted
 * or if no environment is available.
 */
#ifdef _CHI_STANDALONE
#  define CHI_ENV_ENABLED          0
#else
#  define CHI_ENV_ENABLED          1
#endif

#define CHI_CONT_HISTORY_SIZE      0

#ifdef _CHI_STANDALONE
#  define CHI_GC_CONC_ENABLED      0
#else
#  define CHI_GC_CONC_ENABLED      1
#endif

/* -------------------- cby interpreter settings --------------------  */

/**
 * Enable FFI via libffi or dyncall.
 *
 * We don't necessarily need FFI support in the
 * interpreter, since the interpreter is capable of loading natively compiled modules,
 * which could be used for FFI. Using the native FFI is faster, type safer,
 * and more powerful, since it relies on API compatibility by going through the C compiler.
 * In contrast to that, FFI via libffi relies on the ABI, which is more fragile and non portable!
 * However for FFI via libffi a C compiler must not be available.
 */
#if __has_include(<ffi.h>)
#  define CBY_FFI_BACKEND            libffi
#elif __has_include(<dyncall.h>)
#  define CBY_FFI_BACKEND            dyncall
#else
#  define CBY_FFI_BACKEND            none
#endif

/**
 * Enable the instruction dump backend.
 */
#define CBY_BACKEND_DUMP_ENABLED     1

/**
 * Enable the function trace backend,
 * which emits TRACE events for all function calls.
 */
#define CBY_BACKEND_TRACE_ENABLED    1

/**
 * Enable the instruction counting backend
 */
#define CBY_BACKEND_COUNT_ENABLED    1

/**
 * Enable the instruction benchmarking backend,
 * which counts cycles and instructions.
 */
#define CBY_BACKEND_CYCLES_ENABLED   1

/**
 * Enable the instrumenting profiler
 */
#define CBY_BACKEND_CALLS_ENABLED    1

/**
 * Enable the sampling profiler
 */
#define CBY_BACKEND_PROF_ENABLED     1

/**
 * Compile the interpreter with support for bytecode archives
 */
#define CBY_ARCHIVE_ENABLED          1

/**
 * File in the archive which contains name of main module
 */
#define CBY_ARCHIVE_MAIN             "main"

/**
 * Enable the integrated disassembler
 */
#define CBY_DISASM_ENABLED           1

/**
 * Enable module listing
 */
#define CBY_LSMOD_ENABLED            1

/**
 * File extension of shared objects/native module libraries
 */
#define CBY_NATIVE_LIBRARY_EXT       ".so"

/**
 * File extension of module archives
 */
#define CBY_MODULE_ARCHIVE_EXT       ".cbz"

/**
 * File extension of bytecode files
 */
#define CBY_MODULE_EXT               ".cby"

/**
 * Maximum number of FFI arguments
 */
#define CBY_FFI_AMAX                 32

/**
 * Select the dispatching method of the
 * interpreter. Right now direct threading,
 * token threading and switch threading are supported.
 */
#define CBY_DISPATCH                 direct

/**
 * Some architectures have fixed length instructions
 * ARM64, ARM (non thumb) and MIPS32 have 4 byte instructions
 */
#if defined(CHI_ARCH_ARM64) || (defined(CHI_ARCH_ARM) && !defined(__thumb__)) || defined(CHI_ARCH_MIPS32)
#  define CBY_DIRECT_DISPATCH_SHIFT  2
#else
#  define CBY_DIRECT_DISPATCH_SHIFT  0
#endif
