#include <stdlib.h>
#include <sandbox.h>
#include <bits/trap.h>

typedef void (*ctor_t)(void);
extern ctor_t __init_array_start[] __attribute__((weak));
extern ctor_t __init_array_end[] __attribute__((weak));
extern ctor_t __fini_array_start[] __attribute__((weak));
extern ctor_t __fini_array_end[] __attribute__((weak));

static void call_ctors(ctor_t* ctor, ctor_t* end) {
    while (ctor < end)
        (*ctor++)();
}

void __call_ctors(void) {
    call_ctors(__init_array_start, __init_array_end);
}

_Noreturn void exit(int code) {
    __cxa_finalize(0);
    call_ctors(__fini_array_start, __fini_array_end);
    sb_exit(code);
}

_Noreturn void __trap(const char* msg) {
    sb_warn(msg);
    __builtin_trap();
}
