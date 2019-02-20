#pragma once

#include "thread.h"

/**
 * AsyncException tags. Must be ordered alphabetically.
 */
typedef enum {
    CHI_HEAP_OVERFLOW,
    CHI_STACK_OVERFLOW,
    CHI_THREAD_INTERRUPT,
    CHI_USER_INTERRUPT,
} ChiAsyncException;

typedef struct ChiRuntime_ ChiRuntime;

CHI_WU Chili chiAsyncException(ChiAsyncException, Chili);
CHI_WU Chili chiRuntimeException(Chili, Chili);

#define CHI_THROW_RUNTIME_EX(proc, msg)                                 \
    THROW(chiRuntimeException((msg), chiStackGetTrace((proc), SP)))

#define CHI_THROW_ASYNC_EX(proc, type)                                  \
    THROW(chiAsyncException((type), chiStackGetTrace((proc), SP)))
