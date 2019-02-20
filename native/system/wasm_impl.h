#include "../mem.h"
#include "../chunk.h"
#include "../strutil.h"
#include "../runtime.h"
#include <bits/ctors.h>

#define WASM_PAGE_SIZE CHI_KiB(64)

extern unsigned char __heap_base;
static ChiRuntime runtime;

_Noreturn void _start(void);
_Noreturn void _start(void) {
    static bool inited = false;
    if (inited)
        chiWasmContinue();
    inited = true;

    uintptr_t base = CHI_ROUNDUP((uintptr_t)&__heap_base, CHI_CHUNK_MIN_SIZE);
    chiChunkSetup(&(ChiChunkOption){ .arenaStart = base, .arenaSize = (size_t)chiPhysMemory() - base});

    __call_ctors();

    char **argv = (char**)chiAlloc(wasm_args_size());
    int argc = wasm_args_copy(argv);
    chiRuntimeMain(&runtime, argc, argv);
}

void* chiTaskLocal(size_t size, ChiDestructor CHI_UNUSED(destructor)) {
    return chiAlloc(size);
}

void* chiVirtAlloc(void* p, size_t s, int32_t CHI_UNUSED(f)) {
    uintptr_t end = (uintptr_t)p + s, curr = __builtin_wasm_memory_size(0) * WASM_PAGE_SIZE;
    if (end > curr) {
        __builtin_wasm_memory_grow(0, CHI_DIV_ROUNDUP(end - curr, WASM_PAGE_SIZE));
        curr = __builtin_wasm_memory_size(0) * WASM_PAGE_SIZE;
        if (end > curr)
            return 0;
    }
    return p;
}

ChiNanos chiClock(ChiClock CHI_UNUSED(c)) {
    uint64_t t;
    wasm_clock(&t);
    return (ChiNanos){t};
}
