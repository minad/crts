#pragma once

typedef struct ChiRuntime_ ChiRuntime;

typedef enum {
    CHI_HEAPCHECK_NORMAL,
    CHI_HEAPCHECK_PHASECHANGE,
} ChiHeapCheck;

void chiHeapCheck(ChiRuntime*, ChiHeapCheck);
