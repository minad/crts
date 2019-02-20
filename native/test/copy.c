#include "test.h"
#include "../copy.h"

ChiWord dst[32], src[32];

CHI_INL void* asmCopyBytes(void *d, const void *s, size_t n) {
#if defined(NDEBUG) && (defined(CHI_ARCH_X86) || defined(CHI_ARCH_X86_64))
    __asm__ __volatile__ ("rep movsb"
                          : "=D" (d),
                            "=S" (s),
                            "=c" (n)
                          : "0" (d),
                            "1" (s),
                            "2" (n)
                          : "memory");
    return d;
#else
    return memcpy(d, s, n);
#endif
}

// 8 byte words, even on 32 bit platforms!
CHI_INL void* asmCopyWords(void *d, const void *s, size_t n) {
#if defined(NDEBUG) && defined(CHI_ARCH_X86_64)
    __asm__ __volatile__ ("rep movsq"
                          : "=D" (d),
                            "=S" (s),
                            "=c" (n)
                          : "0" (d),
                            "1" (s),
                            "2" (n)
                          : "memory");
    return d;
#elif defined(NDEBUG) && defined(CHI_ARCH_X86)
    n *= 2;
    __asm__ __volatile__ ("rep movsd"
                          : "=D" (d),
                            "=S" (s),
                            "=c" (n)
                          : "0" (d),
                            "1" (s),
                            "2" (n)
                          : "memory");
    return d;
#else
    return memcpy(d, s, CHI_WORDSIZE * n);
#endif
}

static void duffCopy(void* dp, const void* sp, size_t n) {
    ChiWord* d = (ChiWord*)dp;
    const ChiWord* s = (const ChiWord*)sp;
    if (n == 0)
        return;
    size_t m = (n + 7) / 8;
    switch (n % 8) {
    case 0: do { *d++ = *s++; // fallthrough
    case 7:      *d++ = *s++; // fallthrough
    case 6:      *d++ = *s++; // fallthrough
    case 5:      *d++ = *s++; // fallthrough
    case 4:      *d++ = *s++; // fallthrough
    case 3:      *d++ = *s++; // fallthrough
    case 2:      *d++ = *s++; // fallthrough
    case 1:      *d++ = *s++;
            } while (--m > 0);
    }
}

BENCH(chiCopyWords, 1000000) {
    chiCopyWords(dst, src, benchRun & (CHI_DIM(dst) - 1));
}

BENCH(asmCopyWords, 1000000) {
    asmCopyWords(dst, src, benchRun & (CHI_DIM(dst) - 1));
}

BENCH(asmCopyBytes, 1000000) {
    asmCopyBytes(dst, src, sizeof (ChiWord) * (benchRun & (CHI_DIM(dst) - 1)));
}

BENCH(duffCopy, 1000000) {
    duffCopy(dst, src, benchRun & (CHI_DIM(dst) - 1));
}

BENCH(memcpy, 1000000) {
    memcpy(dst, src, sizeof (ChiWord) * (benchRun & (CHI_DIM(dst) - 1)));
}
