#include <libc.h>
#include "../chunk.h"
#include "../mem.h"
#include "../runtime.h"
#include "../strutil.h"

extern unsigned char __heap_base;
static ChiRuntime runtime;

_Noreturn void _start(void);
_Noreturn void _start(void) {
    static bool inited = false;
    if (inited)
        chiWasmContinue();
    inited = true;

    uintptr_t base = CHI_ALIGNUP((uintptr_t)&__heap_base, CHI_CHUNK_MIN_SIZE);
    chiChunkSetup(&(ChiChunkOption){ .arenaStart = base, .arenaSize = (size_t)chiPhysMemory() - base});
    __libc_call_ctors();
    chiSystemSetup();

    char **argv = (char**)chiAlloc(wasm_args_size());
    int argc = wasm_args_copy(argv);
    chiRuntimeMain(&runtime, argc, argv, exit);
}

bool chiVirtCommit(ChiVirtMem* CHI_UNUSED(mem), void* ptr, size_t size, bool CHI_UNUSED(huge)) {
    CHI_ASSERT(ptr);
    CHI_ASSERT(size);
    uintptr_t end = (uintptr_t)ptr + size, curr = wasm_memory_size();
    if (end > curr) {
        wasm_memory_grow(end - curr);
        curr = wasm_memory_size();
        if (end > curr)
            return false;
    }
    return true;
}
