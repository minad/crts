/*
 * Based on musl libc by Rich Felker. MIT licensed.
 *
 * We provide mem* functions, which rely on auto vectorization.
 * These functions are performance critical.
 * In contrast, the str* functions are naive,
 * since they are operate on legacy c strings and
 * do not play a significant role in the Chili runtime.
 */
#include <stdint.h>
#include <string.h>

typedef uintptr_t word_t;
#define WSIZE        (sizeof (word_t))
#define WALIGNED(p)  (!(((uintptr_t)p) & (WSIZE - 1)))
#define CONSTCAST(p) ((void*)(uintptr_t)(p))

// Enable vectorizer, but ensure that functions are not self-referential
#ifndef __clang__
#  pragma GCC push_options
#  pragma GCC optimize("O3,no-tree-loop-distribute-patterns")
#endif

void* memset(void* dst, int x, size_t n) {
    char c = (char)x;
    word_t w = ((word_t)-1 / 255) * (unsigned char)c;
    char* d = (char*)dst;

    while (!WALIGNED(d) && n) {
        *d++ = c;
        --n;
    }

    while (n >= 4 * WSIZE) {
        word_t* dw = (word_t*)(void*)d;
        dw[0] = w; dw[1] = w; dw[2] = w; dw[3] = w;
        d += 4 * WSIZE;
        n -= 4 * WSIZE;
    }

    while (n >= WSIZE) {
        word_t* dw = (word_t*)(void*)d;
        dw[0] = w;
        d += WSIZE;
        n -= WSIZE;
    }

    while (n) {
        *d++ = c;
        --n;
    }

    return dst;
}

static inline void* memcpy_forward(void* dst, const void* src, size_t n) {
    char* d = (char*)dst;
    const char* s = (const char*)src;

    while (!WALIGNED(d) && n) {
        *d++ = *s++;
        --n;
    }

    if (WALIGNED(s)) {
        while (n >= 4 * WSIZE) {
            word_t* dw = (word_t*)(void*)d;
            const word_t* sw = (const word_t*)(const void*)s;
            dw[0] = sw[0]; dw[1] = sw[1]; dw[2] = sw[2]; dw[3] = sw[3];
            d += 4 * WSIZE;
            s += 4 * WSIZE;
            n -= 4 * WSIZE;
        }

        while (n >= WSIZE) {
            word_t* dw = (word_t*)(void*)d;
            const word_t* sw = (const word_t*)(const void*)s;
            dw[0] = sw[0];
            d += WSIZE;
            s += WSIZE;
            n -= WSIZE;
        }
    }

    while (n) {
        *d++ = *s++;
        --n;
    }

    return dst;
}

static inline void* memcpy_backward(void* dst, const void* src, size_t n) {
    char* d = (char*)dst + n;
    const char* s = (const char*)src + n;

    while (!WALIGNED(d) && n) {
        *--d = *--s;
        --n;
    }

    if (WALIGNED(s)) {
        while (n >= 4 * WSIZE) {
            d -= 4 * WSIZE;
            s -= 4 * WSIZE;
            n -= 4 * WSIZE;
            word_t* dw = (word_t*)(void*)d;
            const word_t* sw = (const word_t*)(const void*)s;
            dw[0] = sw[0]; dw[1] = sw[1]; dw[2] = sw[2]; dw[3] = sw[3];
        }

        while (n >= WSIZE) {
            d -= WSIZE;
            s -= WSIZE;
            n -= WSIZE;
            word_t* dw = (word_t*)(void*)d;
            const word_t* sw = (const word_t*)(const void*)s;
            dw[0] = sw[0];
        }
    }

    while (n) {
        *--d = *--s;
        --n;
    }

    return dst;
}

void* memcpy(void* restrict dst, const void* restrict src, size_t n) {
    return memcpy_forward(dst, src, n);
}

void* memmove(void* dst, const void* src, size_t n) {
    return (const char*)dst < (const char*)src ? memcpy_forward(dst, src, n) : memcpy_backward(dst, src, n);
}

int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* p = (const unsigned char*)a, *q = (const unsigned char*)b;
    while (!WALIGNED(p) && n && *p == *q) {
        ++p;
        ++q;
        --n;
    }
    if (WALIGNED(q)) {
        while (n >= 4 * WSIZE) {
            const word_t *pw = (const word_t*)(const void*)p, *qw = (const word_t*)(const void*)q;
            if ((pw[0] - qw[0]) | (pw[1] - qw[1]) | (pw[2] - qw[2]) | (pw[3] - qw[3]))
                break;
            p += 4 * WSIZE;
            q += 4 * WSIZE;
            n -= 4 * WSIZE;
        }
        while (n >= WSIZE) {
            const word_t *pw = (const word_t*)(const void*)p, *qw = (const word_t*)(const void*)q;
            if (pw[0] != qw[0])
                break;
            p += WSIZE;
            q += WSIZE;
            n -= WSIZE;
        }
    }
    while (n && *p == *q) {
        ++p;
        ++q;
        --n;
    }
    return n ? *p - *q : 0;
}

size_t strlen(const char* s) {
    const char* p = s;
    while (*p)
        ++p;
    return (size_t)(p - s);
}

int strcmp(const char* p, const char* q) {
    while (*p && *p == *q) {
        ++p;
        ++q;
    }
    return *(const unsigned char*)p - *(const unsigned char*)q;
}

int strncmp(const char* p, const char* q, size_t n) {
    if (!n)
        return 0;
    while (--n && *p && *q && *p == *q) {
        ++p;
        ++q;
    }
    return *(const unsigned char*)p - *(const unsigned char*)q;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == c)
            return (char*)CONSTCAST(s);
        ++s;
    }
    return 0;
}

char* strrchr(const char* s, int c) {
    return (char*)memrchr(s, c, strlen(s));
}

void* memrchr(const void* mem, int c, size_t n) {
    const char* p = (const char*)mem + n;
    while (n--) {
        if (*--p == c)
            return CONSTCAST(p);
    }
    return 0;
}

void* memchr(const void* mem, int c, size_t n) {
    const char* p = (const char*)mem;
    while (n--) {
        if (*p == c)
            return CONSTCAST(p);
        ++p;
    }
    return 0;
}

void* memccpy(void* restrict dst, const void* restrict src, int c, size_t n) {
    char* d = (char*)dst;
    const char* s = (const char*)src;
    while (n--) {
        *d++ = *s;
        if (*s++ == c)
            return d;
    }
    return 0;
}

#ifndef __clang__
#  pragma GCC pop_options
#endif
