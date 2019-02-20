#if defined (__x86_64__)
#  define CHI_ARCH_X86_64    1
#  define CHI_ARCH           x86_64
#  define CHI_ARCH_BITS      64
#elif defined (__i386__)
#  define CHI_ARCH_X86       1
#  define CHI_ARCH           x86
#  define CHI_ARCH_BITS      32
#elif defined (__aarch64__)
#  define CHI_ARCH_ARM64     1
#  define CHI_ARCH           arm64
#  define CHI_ARCH_BITS      64
#elif defined (__sparc__) && defined(__arch64__)
#  define CHI_ARCH_SPARC64   1
#  define CHI_ARCH           sparc64
#  define CHI_ARCH_BITS      64
#elif defined (__arm__)
#  define CHI_ARCH_ARM32     1
#  define CHI_ARCH           arm32
#  define CHI_ARCH_BITS      32
#elif defined (__powerpc64__)
#  define CHI_ARCH_PPC64     1
#  define CHI_ARCH           ppc64
#  define CHI_ARCH_BITS      64
#elif defined (__powerpc__)
#  define CHI_ARCH_PPC32     1
#  define CHI_ARCH           ppc32
#  define CHI_ARCH_BITS      32
#elif defined (__riscv)
#  if !defined(__riscv_float_abi_double)
#    error riscv d extension is required
#  endif
#  if !defined(__riscv_atomic)
#    error riscv a extension is required
#  endif
#  if __riscv_xlen == 64
#    define CHI_ARCH_RISCV64   1
#    define CHI_ARCH           riscv64
#    define CHI_ARCH_BITS      64
#  else
#    define CHI_ARCH_RISCV32   1
#    define CHI_ARCH           riscv32
#    define CHI_ARCH_BITS      32
#  endif
#elif defined (__mips__)
#  define CHI_ARCH_MIPS32    1
#  define CHI_ARCH           mips32
#  define CHI_ARCH_BITS      32
#elif defined (__wasm__)
#  define CHI_ARCH_WASM32    1
#  define CHI_ARCH           wasm32
#  define CHI_ARCH_BITS      32
#else
#  error Unsupported architecture
#endif
