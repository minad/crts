#pragma once

#include <stddef.h>

_Noreturn void abort(void);
_Noreturn void exit(int);
__attribute__((malloc)) void* malloc(size_t);
__attribute__((malloc)) void* calloc(size_t, size_t);
__attribute__((malloc)) void* realloc(void*, size_t);
void free(void*);
