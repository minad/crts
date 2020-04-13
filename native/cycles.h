#pragma once

#include "system.h"

// Taken from https://github.com/google/benchmark/blob/master/src/cycleclock.h
// LICENSE: Apache 2.0
CHI_INL CHI_WU uint64_t chiCpuCycles(void) {
#if defined(__clang__)
    return __builtin_readcyclecounter();
#elif defined(CHI_ARCH_X86)
    uint64_t reg;
    __asm__ __volatile__ ("rdtsc" : "=A"(reg));
    return reg;
#elif defined(CHI_ARCH_X86_64)
    uint64_t rax, rdx;
    __asm__ __volatile__ ("rdtsc" : "=a"(rax), "=d"(rdx));
    return (rdx << 32) | rax;
#elif defined(CHI_ARCH_ARM32)
    uint32_t reg;
    __asm__ __volatile__ ("mrc p15, 0, %0, c9, c14, 0" : "=r"(reg));
    if (reg & 1) {  // Counter access permitted
        __asm__ __volatile__ ("mrc p15, 0, %0, c9, c12, 1" : "=r"(reg));
        if (reg & 0x80000000UL) {  // Counter active
            __asm__ __volatile__ ("mrc p15, 0, %0, c9, c13, 0" : "=r"(reg));
            return (uint64_t)reg * 64;
        }
    }
    return CHI_UN(Nanos, chiClockMonotonicFine());
#elif defined(CHI_ARCH_ARM64)
    uint64_t reg;
    __asm__ __volatile__ ("mrs %0, cntvct_el0" : "=r"(reg));
    return reg;
#elif defined(CHI_ARCH_SPARC64)
    uint64_t reg;
    __asm__ __volatile__ (".byte 0x83, 0x41, 0x00, 0x00");
    __asm__ __volatile__ ("mov %%g1, %0" : "=r"(reg));
    return reg;
#elif defined(CHI_ARCH_RISCV64)
    uint64_t reg;
    __asm__ __volatile__ ("rdtime %0" : "=r" (reg));
    return reg;
#elif defined(CHI_ARCH_RISCV32)
    uint32_t lo, hi, hi2;
    __asm__ __volatile__ ("1: rdtimeh %0\n"
                          "   rdtime  %1\n"
                          "   rdtimeh %2\n"
                          "   bne     %0, %2, 1b"
                          : "=&r" (hi), "=&r" (lo), "=&r" (hi2));
    return ((u64)hi << 32) | lo;
#elif defined(CHI_ARCH_PPC32) || defined(CHI_ARCH_PPC64)
    int64_t tbl, tbu0, tbu1;
    __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
    __asm__ __volatile__ ("mftb  %0" : "=r"(tbl));
    __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
    tbl &= -(int64_t)(tbu0 == tbu1);
    return (uint64_t)((tbu1 << 32) | tbl);
#elif defined(CHI_ARCH_WASM32)
    return CHI_UN(Nanos, chiClockMonotonicFine());
#else
#  warning Unsupported architecture. chiCpuCycles uses clock as fallback.
    return CHI_UN(Nanos, chiClockMonotonicFine());
#endif
}

CHI_INTERN CHI_WU uint32_t chiCpuCyclesOverhead(void);
