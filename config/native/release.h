/*
 * Configuration file for the native Chili runtime.
 */

#include "compiler.h"

#if defined(CHI_STANDALONE_SANDBOX) || defined(CHI_STANDALONE_WASM)
#  define _CHI_STANDALONE
#endif

/**
 * System characteristics. The runtime can be
 * compiled on standalone systems without libc.
 */
#if defined(_CHI_STANDALONE)
#  define CHI_SYSTEM_HAS_STATS              0
#  define CHI_SYSTEM_HAS_ERRNO              0
#  define CHI_SYSTEM_HAS_MALLOC             0
#  define CHI_SYSTEM_HAS_INTERRUPT          0
#  define CHI_SYSTEM_HAS_TASK               0
#  define CHI_SYSTEM_HAS_VIRTALLOC          0
#  define CHI_SYSTEM_HAS_PARTIAL_VIRTFREE   0
#else
#  define CHI_SYSTEM_HAS_STATS              1
#  define CHI_SYSTEM_HAS_ERRNO              1
#  define CHI_SYSTEM_HAS_MALLOC             1
#  define CHI_SYSTEM_HAS_INTERRUPT          1
#  define CHI_SYSTEM_HAS_TASK               1
#  define CHI_SYSTEM_HAS_VIRTALLOC          1
#  if defined(_WIN32)
#    define CHI_SYSTEM_HAS_PARTIAL_VIRTFREE 1
#  else
#    define CHI_SYSTEM_HAS_PARTIAL_VIRTFREE 0
#  endif
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
#define CHI_PAGER                  "/usr/bin/less --LONG-PROMPT --quit-if-one-screen --no-init --raw-control-chars"

/**
 * Use interrupts for timeouts instead of polling
 */
#define CHI_TIMEOUT_INTERRUPT_ENABLED CHI_SYSTEM_HAS_INTERRUPT

/**
 * Inlining aggressiveness
 */
/*#define CHI_INL static __attribute__ ((unused))*/
/*#define CHI_INL static inline*/
#define CHI_INL static inline __attribute__ ((always_inline))

/* -------------------- Calling convention, continuation settings --------------------  */

/**
 * Select the calling convention
 */
#if defined(CHI_ARCH_X86_64) && __has_include(<__tailcall__.h>)
#  include <__tailcall__.h>
#  ifdef __clang__
#    define CHI_CALLCONV           tail_stack_arg10
#  else
#    define CHI_CALLCONV           tail_stack_arg6
#  endif
#elif defined(CHI_STANDALONE_WASM)
#  ifdef __wasm_tail_call__
#    define CHI_CALLCONV           tail_tls
#  else
#    define CHI_CALLCONV           trampoline_tls
#endif
#else
#  define CHI_CALLCONV             trampoline_stack
#endif

/**
 * Enable location information for stack traces
 */
#define CHI_LOC_ENABLED            1

/**
 * On 32 bit platforms, prefix data is not needed,
 * since the function pointer and the continuation info
 * can both be packed into the 64 bit Chili value.
 *
 * Coincidentally, wasm32 does not support mixed data and code since it
 * is a Harvard architecture.
 */
#if CHI_ARCH_32BIT
#  define CHI_CONT_PREFIX             0
#else
#  define CHI_CONT_PREFIX             1
#  if defined(__clang__) && __has_include(<__prefix_data__.h>)
#    include <__prefix_data__.h>
#    define CHI_CONT_PREFIX_SECTION   0
#    define CHI_CONT_PREFIX_ALIGN     16
#  else
#    define CHI_CONT_PREFIX_SECTION   1
#    ifdef _WIN32
#      define CHI_CONT_PREFIX_ALIGN   32
#    else
#      define CHI_CONT_PREFIX_ALIGN   16
#    endif
#  endif
#endif

/* -------------------- Basic container/algorithm settings --------------------  */

/**
 * Hash table maximum filling in percent
 */
#define CHI_HT_MAXFILL             80

/**
 * Hash table initial size
 */
#define CHI_HT_INITSHIFT           8 /* size = 2 << shift */

/**
 * Dynamic vector growth factor in percent
 */
#define CHI_VEC_GROWTH             200

/**
 * Quick sort cutoff, below insertion sort is used
 */
#define CHI_QUICK_SORT_CUTOFF      16

/* -------------------- Event system settings --------------------  */

#define CHI_STATS_ENABLED          1        /** Enable the statistics system */
#define CHI_EVENT_ENABLED          1        /** Enable the event system */
#define CHI_COUNT_ENABLED          CHI_EVENT_ENABLED
#define CHI_EVENT_PRETTY_ENABLED   1        /** Enable the pretty log writer */

#if defined(_CHI_STANDALONE)
#  define CHI_EVENT_CONVERT_ENABLED  0
#else
#  define CHI_EVENT_CONVERT_ENABLED  1
#endif

/**
 * Enable DTrace probes
 */
#define CHI_DTRACE_ENABLED         0

/**
 * Enable LTTng trace points
 */
#define CHI_LTTNG_ENABLED          0

/* -------------------- Heap settings --------------------  */

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
 * Experimental setting to to compress 64 bit double precision
 * floating points to 63 bits, such that the values can always be stored unboxed.
 */
#define CHI_FLOAT63_ENABLED     0

/**
 * Minimum block size in words
 */
#define CHI_BLOCK_MINSIZE          512 /* 4K */

/**
 * Maximum block size in words
 */
#define CHI_BLOCK_MAXSIZE          131072 /* 1M */

/**
 * Enable small string optimization.
 * Small strings are stored unboxed if enabled.
 */
#define CHI_STRING_UNBOXING        1

/**
 * Enable small integer optimization
 * for big integers. Small integers
 * are stored unboxed if enabled.
 */
#define CHI_BIGINT_UNBOXING        1

/**
 * Enable unboxing for Int64/UInt64.
 * This option is useful to measure
 * the boxing overhead.
 */
#define CHI_INT64_UNBOXING         1

/**
 * Use GMP for big integer acceleration
 */
#if __has_include(<gmp.h>)
#  define CHI_BIGINT_GMP           1
#else
#  define CHI_BIGINT_GMP           0
#endif

/**
 * Big integer size limit in bytes.
 */
#define CHI_BIGINT_LIMIT           102400 /* 100K */

/**
 * Enable the concurrent mark and sweep
 * garbage collector
 */
#define CHI_GC_CONC_ENABLED       CHI_SYSTEM_HAS_TASK

/* -------------------- Chunk allocator settings --------------------  */

/**
 * Enable a fixed chunk memory arena.
 * This partially defeats ASLR.
 * On 64 bit platforms, this setting must be enabled for NaN boxing.
 * See CHI_NANBOXING_ENABLED.
 */
#ifdef _CHI_STANDALONE
#  define CHI_CHUNK_ARENA_ENABLED   1
#  define CHI_CHUNK_ARENA_FIXED     0
#elif !CHI_ARCH_32BIT && CHI_NANBOXING_ENABLED
#  define CHI_CHUNK_ARENA_ENABLED   1
#  define CHI_CHUNK_ARENA_FIXED     1
#else
#  define CHI_CHUNK_ARENA_ENABLED   0
#  define CHI_CHUNK_ARENA_FIXED     0
#endif

#if CHI_CHUNK_ARENA_FIXED
#  define CHI_CHUNK_MAX_SHIFT     37 /* 128G */
#  define CHI_CHUNK_START         ((uintptr_t)1 << 37) /* start address (bytes) */
#else
#  if CHI_ARCH_32BIT
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
#define CHI_CHUNK_VIRTALLOC_TRIES 10

/**
 * Enable the memory unmapper background task.
 * Returning memory to the OS is usually slow,
 * therefore the task is needed.
 * Memory is not returned immediately but
 * only after a certain timeout.
 */
#define CHI_UNMAP_TASK_ENABLED    CHI_SYSTEM_HAS_TASK

/* -------------------- Debugging and memory poisoning --------------------  */

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

#define CHI_POISON_BLOCK_CANARY      UINT64_C(0xB10CB10CB10CB10C)
#define CHI_POISON_BLOCK_FREE        0xA5
#define CHI_POISON_BLOCK_USED        0x5A
#define CHI_POISON_LIST              0x4c495354
#define CHI_POISON_DESTROYED         0xDE
#define CHI_POISON_UNDEF             0xDF
#define CHI_POISON_MALLOC_BEGIN      UINT64_C(0x585843414e415259)
#define CHI_POISON_MALLOC_END        UINT64_C(0x5952414e41435858)
#define CHI_POISON_STACK_CANARY      UINT64_C(0xDADAD0D0DADAD0D0)
#define CHI_POISON_STACK_CANARY_SIZE 0

/*
 * Additional stack debugging settings
 */

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

/**
 * Enable function logging
 */
#define CHI_FNLOG_ENABLED          0

/* -------------------- Sink settings --------------------  */

/**
 * Enable colors
 */
#define CHI_COLOR_ENABLED          1

/* -------------------- Other runtime features --------------------  */

/**
 * Profiler
 */
#define CHI_PROF_ENABLED              CHI_CBY_SUPPORT_ENABLED

/**
 * Enable tracepoints in the runtime.
 */
#define CHI_TRACEPOINTS_ENABLED       CHI_PROF_ENABLED

/**
 * Enable tracepoints in native code for enhanced profiling.
 * Without tracepoints native code won't appear in the profiler output!
 */
#define CHI_TRACEPOINTS_CONT_ENABLED  0

/**
 * Enable the command line parser for the runtime options.
 */
#define CHI_OPTION_ENABLED         1

/* -------------------- cby interpreter settings --------------------  */

/**
 * Compile the runtime with support for the interpreter
 */
#ifndef CHI_CBY_SUPPORT_ENABLED
#  define CHI_CBY_SUPPORT_ENABLED 0
#endif

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
 * Enable the function logging backend,
 * which emits FNLOG events for all function calls.
 */
#define CBY_BACKEND_FNLOG_ENABLED    1

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
#if __has_include(<zlib.h>)
#  define CBY_ARCHIVE_ENABLED            1
#  define CBY_CRC32_ENABLED          1
#else
#  define CBY_ARCHIVE_ENABLED            0
#  define CBY_CRC32_ENABLED          0
#endif

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
 * File extension of native dynamic libraries
 */
#ifdef _WIN32
#  define CBY_DYNLIB_EXT             ".dll"
#else
#  define CBY_DYNLIB_EXT             ".so"
#endif

/**
 * File extension of module archives
 */
#define CBY_ARCHIVE_EXT                  ".cbz"

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
