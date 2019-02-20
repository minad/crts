#include "debug.h"
#include "sink.h"
#include "error.h"
#include <stdarg.h>
#include <stdlib.h>
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

#if CHI_POISON_BLOCK_ENABLED
CHI_NOINL void chiPoisonBlock(void* x, uint8_t p, size_t n) {
    memset(x, p, n);
    VALGRIND_MAKE_MEM_UNDEFINED(x, n);
}
#endif

#if CHI_POISON_OBJECT_ENABLED
CHI_NOINL void chiPoisonObject(void* p, size_t n) {
    for (size_t i = 0; i < n; ++i)
        i[(Chili*)p] = chiWrapUnchecked((void*)CHI_CHUNK_START, 0, CHI_POISON_OBJECT);
}
#endif

#ifndef NDEBUG
_Noreturn void chiAssert(const char* file, uint32_t line, const char* func, const char* cond) {
    chiBug(file, line, func, "Assertion `%s' failed", cond);
}

_Noreturn void chiBug(const char* file, uint32_t line, const char* func, const char* fmt, ...) {
    chiSinkFmt(chiStderr, "Error: %s:%u: %s: ", file, line, func);
    va_list ap;
    va_start(ap, fmt);
    chiSinkFmtv(chiStderr, fmt, ap);
    va_end(ap);
    chiSinkPutc(chiStderr, '\n');
    chiAbort("Abort!");
}
#endif
