#include "error.h"
#include "sink.h"
#include "system.h"

CHI_COLD void chiWarn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkWarnv(chiStderr, fmt, ap);
    va_end(ap);
}

CHI_COLD _Noreturn void chiErr(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkWarnv(chiStderr, fmt, ap);
    va_end(ap);
    exit(CHI_EXIT_FATAL);
}

CHI_COLD _Noreturn void chiSysErr(const char* name) {
    chiErr("%s failed: %m", name);
}

CHI_COLD _Noreturn void chiAbort(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkWarnv(chiStderr, fmt, ap);
    va_end(ap);
    __builtin_trap();
}
