#pragma once

#include <stdint.h>
#include <stddef.h>

enum {
    WASM_EXIT,
    WASM_SUSPEND,
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
