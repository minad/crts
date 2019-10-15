#pragma once

#include "../bytecode/opcodes.h"
#include "native/cycles.h"

typedef struct {
    uint64_t begin, interp, insnBegin, insn[OPCODE_COUNT];
} cyclesData;

CHI_INL void cyclesAlloc(ChiProcessor* CHI_UNUSED(proc), cyclesData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void cyclesFFITail(ChiProcessor* CHI_UNUSED(proc), cyclesData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void cyclesFFIBegin(ChiProcessor* CHI_UNUSED(proc), cyclesData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void cyclesFFIEnd(ChiProcessor* CHI_UNUSED(proc), cyclesData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void cyclesDestroy(CbyInterpreter* CHI_UNUSED(interp), cyclesData* CHI_UNUSED(global)) {}
CHI_INL void cyclesProcStart(ChiProcessor* CHI_UNUSED(proc), cyclesData* CHI_UNUSED(local), cyclesData* CHI_UNUSED(global)) {}
CHI_INL void cyclesInsnBegin(ChiProcessor* CHI_UNUSED(proc), cyclesData* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op)) {}

CHI_INL void cyclesEnter(ChiProcessor* CHI_UNUSED(proc), cyclesData *local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), bool CHI_UNUSED(jmp)) {
    uint64_t c = chiCpuCycles();
    local->insnBegin = c;
    local->interp = c - local->interp;
}

CHI_INL void cyclesLeave(ChiProcessor* CHI_UNUSED(proc), cyclesData *local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {
    local->interp = chiCpuCycles() - local->interp;
}

CHI_INL void cyclesInsnEnd(ChiProcessor* CHI_UNUSED(proc), cyclesData *local, Opcode op) {
    uint64_t c = chiCpuCycles();
    local->insn[op] += c - local->insnBegin;
    local->insnBegin = c;
}

CHI_INL void cyclesProcStop(ChiProcessor* CHI_UNUSED(proc), cyclesData *local, cyclesData *global) {
    global->interp += local->interp;
    for (size_t i = 0; i < OPCODE_COUNT; ++i)
        global->insn[i] += local->insn[i];
}

CHI_INL void cyclesSetup(CbyInterpreter* CHI_UNUSED(interp), cyclesData *global) {
    global->begin = chiCpuCycles();
}
