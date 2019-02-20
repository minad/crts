#pragma once
#include <chili/object/bigint.h>

typedef struct {
    char* buf;
    size_t size;
} ChiBigIntBuf;

void chiBigIntSetup(void);
const char* chiBigIntStr(Chili, ChiBigIntBuf*);
