#include "error.h"
#include "sink.h"

void chiWarn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkWarnv(chiStderr, fmt, ap);
    va_end(ap);
}

_Noreturn void chiErr(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkWarnv(chiStderr, fmt, ap);
    va_end(ap);
#ifdef NDEBUG
    exit(CHI_EXIT_FATAL);
#else
    __builtin_trap();
#endif
}

_Noreturn void chiSysErr(const char* name) {
    chiErr("%s failed: %m", name);
}
