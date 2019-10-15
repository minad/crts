#include <libc.h>
#include <stdlib.h>
#include <string.h>
#include <wasm.h>

typedef struct {
    void (*fn)(void*);
    void* arg;
} dtor_t;

static dtor_t* dtor_entry = 0;
static size_t dtor_capacity = 0, dtor_count = 0;

void __wasm_call_ctors(void);
int __cxa_atexit(void(*)(void*), void*, void*);

// For some reason, in wasm, destructors are lowered to constructors calling __cxa_atexit.
int __cxa_atexit(void (*fn)(void*), void* arg, void* dso) {
    // TODO: it might be necessary to lock __cxa_atexit on multithreaded wasm
    (void)dso;
    if (dtor_count >= dtor_capacity) {
        dtor_capacity = dtor_capacity ? 2 * dtor_capacity : 8;
        dtor_entry = (dtor_t*)realloc(dtor_entry, sizeof (dtor_t) * dtor_capacity);
    }
    dtor_entry[dtor_count++] = (dtor_t){ fn, arg };
    return 0;
}

void __libc_call_ctors(void) {
    __wasm_call_ctors();
}

static void __libc_call_dtors(void) {
    for (size_t i = dtor_count; i --> 0;)
        dtor_entry[i].fn(dtor_entry[i].arg);
}

_Noreturn void exit(int code) {
    __libc_call_dtors();
    wasm_throw(&(wasm_exception_t){WASM_EXIT,(uint32_t)code});
}

_Noreturn void __libc_trap(const char* s) {
    wasm_sink_write(2, s, strlen(s));
    __builtin_trap();
}
