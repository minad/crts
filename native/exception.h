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

CHI_INTERN CHI_WU Chili chiAsyncException(ChiProcessor*, Chili*, ChiAsyncException);
CHI_INTERN CHI_WU Chili chiRuntimeException(ChiProcessor*, Chili*, Chili);

#define CHI_THROW_RUNTIME_EX(proc, msg) ({ A(0) = chiRuntimeException((proc), SP, (msg)); KNOWN_JUMP(chiExceptionThrow); })
#define CHI_THROW_ASYNC_EX(proc, type) ({ A(0) = chiAsyncException((proc), SP, (type)); KNOWN_JUMP(chiExceptionThrow); })
