#if defined (__x86_64__)
#  define CHI_ARCH_X86_64
#  define CHI_ARCH           x86_64
#  define CHI_ARCH_32BIT     0
#  define CHI_ARCH_TRAP_OP   0xCCCCCCCC
#elif defined (__i386__)
#  define CHI_ARCH_X86
#  define CHI_ARCH           x86
#  define CHI_ARCH_32BIT     1
#  define CHI_ARCH_TRAP_OP   0x00000B0F
#elif defined (__aarch64__)
#  define CHI_ARCH_ARM64
#  define CHI_ARCH           arm64
#  define CHI_ARCH_32BIT     0
#  define CHI_ARCH_TRAP_OP   0xd4207d00
#elif defined (__arm__)
#  define CHI_ARCH_ARM32
#  define CHI_ARCH           arm32
#  define CHI_ARCH_32BIT     1
#  define CHI_ARCH_TRAP_OP   0xe7f000f0
#elif defined (__powerpc64__)
#  define CHI_ARCH_PPC64
#  define CHI_ARCH           ppc64
#  define CHI_ARCH_32BIT     0
#  define CHI_ARCH_TRAP_OP   0x7fe00008
#elif defined (__powerpc__)
#  define CHI_ARCH_PPC32
#  define CHI_ARCH           ppc32
#  define CHI_ARCH_32BIT     1
#  define CHI_ARCH_TRAP_OP   0x7fe00008
#elif defined (__mips__)
#  define CHI_ARCH_MIPS32
#  define CHI_ARCH           mips32
#  define CHI_ARCH_32BIT     1
#  define CHI_ARCH_TRAP_OP   0x0000000d
#elif defined (__wasm__)
#  define CHI_ARCH_WASM32
#  define CHI_ARCH           wasm32
#  define CHI_ARCH_32BIT     1
#elif defined (__riscv)
#  if !defined(__riscv_float_abi_double)
#    error riscv d extension is required
#  endif
#  if !defined(__riscv_atomic)
#    error riscv a extension is required
#  endif
#  if __riscv_xlen == 64
#    define CHI_ARCH_RISCV64
#    define CHI_ARCH           riscv64
#    define CHI_ARCH_32BIT     0
#  else
#    define CHI_ARCH_RISCV32
#    define CHI_ARCH           riscv32
#    define CHI_ARCH_32BIT     1
#  endif
#  define CHI_ARCH_TRAP_OP     0x00100073 // TODO: Check
#else
#  error Unsupported architecture
#endif
