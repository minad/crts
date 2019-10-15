#include <libc.h>
#include <sandbox.h>
#include <stdlib.h>

typedef void (*ctor_t)(void);
extern ctor_t __init_array_start[] __attribute__((weak));
extern ctor_t __init_array_end[] __attribute__((weak));
extern ctor_t __fini_array_start[] __attribute__((weak));
extern ctor_t __fini_array_end[] __attribute__((weak));

static void call_ctors(ctor_t* ctor, ctor_t* end) {
    while (ctor < end)
        (*ctor++)();
}

void __libc_call_ctors(void) {
    call_ctors(__init_array_start, __init_array_end);
}

static void __libc_call_dtors(void) {
    call_ctors(__fini_array_start, __fini_array_end);
}

_Noreturn void exit(int code) {
    __libc_call_dtors();
    sb_exit(code);
}

_Noreturn void __libc_trap(const char* msg) {
    sb_warn(msg);
    __builtin_trap();
}
