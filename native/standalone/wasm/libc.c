#include <stdlib.h>
#include <string.h>
#include <wasm.h>
#include <bits/trap.h>

void __wasm_call_ctors(void);

void __call_ctors(void) {
    __wasm_call_ctors();
}

_Noreturn void exit(int code) {
    __cxa_finalize(0);
    wasm_throw(&(wasm_exception_t){WASM_EXIT,(uint32_t)code});
}

_Noreturn void __trap(const char* s) {
    wasm_sink_write(2, s, strlen(s));
    __builtin_trap();
}
