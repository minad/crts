#pragma once

#include <stddef.h>
#include <stdint.h>

#define WASM_PAGE_SIZE 0x10000

enum {
    WASM_EXIT,
    WASM_PARK,
};

typedef struct {
    uint32_t id, data;
} wasm_exception_t;

uint32_t wasm_sink_open(const char*);
void wasm_sink_close(uint32_t);
size_t wasm_sink_write(uint32_t, const void*, size_t);
void wasm_sink_write_all(uint32_t, const void*, size_t);
int wasm_args_copy(void*);
size_t wasm_args_size(void);
void wasm_clock(uint64_t*);
_Noreturn void wasm_throw(const wasm_exception_t*);

static inline size_t wasm_memory_size(void) {
    return WASM_PAGE_SIZE * __builtin_wasm_memory_size(0);
}

static inline void wasm_memory_grow(size_t size) {
    __builtin_wasm_memory_grow(0, (size + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE);
}
