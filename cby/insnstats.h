#pragma once

#include "bytecode/opcodes.h"
#include "cby.h"

typedef struct {
    uint64_t begin, interp, insnBegin, insn[OPCODE_COUNT];
} CbyCyclesStats;

typedef struct {
    uint64_t enter, leave, enterJmp;
    uint64_t insn[OPCODE_COUNT];
} CbyCountStats;

void cbyPrintInsnTable(CbyInterpreter*, const CbyCountStats*, const CbyCyclesStats*);
