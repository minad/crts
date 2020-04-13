#include <stdlib.h>
#include "debug.h"
#include "error.h"
#include "sink.h"
#if CHI_VALGRIND_ENABLED
#  include <valgrind/memcheck.h>
#else
#  define VALGRIND_MAKE_MEM_UNDEFINED(x, y) ({})
#endif

/**
 * Separate poison function to generate separate profiling/debug records.
 */
#if CHI_POISON_ENABLED
CHI_NOINL void chiPoisonMem(void* x, uint8_t p, size_t n) {
    memset(x, p, n);
    VALGRIND_MAKE_MEM_UNDEFINED(x, n);
}
#endif

#if CHI_POISON_OBJECT_ENABLED
CHI_NOINL void chiPoisonObject(void* p, size_t n) {
    for (size_t i = 0; i < n; ++i)
        i[(Chili*)p] = _CHILI_POISON;
}
#endif

#ifndef NDEBUG
static void debugMsgv(const char* prefix, const char* file, uint32_t line, const char* func, const char* fmt, va_list ap) {
    char buf[512];
    ChiSinkMem s;
    chiSinkMemInit(&s, buf, sizeof (buf));
    chiSinkFmt(&s.base, "%s: %s:%u: %s: ", prefix, file, line, func);
    chiSinkFmtv(&s.base, fmt, ap);
    s.used = CHI_MIN(s.capacity - 1, s.used);
    chiSinkPutc(&s.base, '\n');
    chiSinkWrite(chiStderr, s.buf, s.used);
}

static void debugMsg(const char* prefix, const char* file, uint32_t line, const char* func, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    debugMsgv(prefix, file, line, func, fmt, ap);
    va_end(ap);
}

void chiDebugMsg(const char* file, uint32_t line, const char* func, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    debugMsgv("Debug", file, line, func, fmt, ap);
    va_end(ap);
}

void chiDebugBug(const char* file, uint32_t line, const char* func, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    debugMsgv("Bug", file, line, func, fmt, ap);
    va_end(ap);
    __builtin_trap();
}

void chiDebugAssert(const char* file, uint32_t line, const char* func, const char* cond) {
    debugMsg("Assert", file, line, func, "`%s' failed", cond);
    __builtin_trap();
}
#endif
