#pragma once

#include "../bytecode/opcodes.h"
#include "../cby.h"

typedef struct {
    uint64_t enter, leave, enterJmp;
    uint64_t insn[OPCODE_COUNT];
} countData;

CHI_INL void countAlloc(ChiProcessor* CHI_UNUSED(proc), countData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void countFFITail(ChiProcessor* CHI_UNUSED(proc), countData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void countFFIBegin(ChiProcessor* CHI_UNUSED(proc), countData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void countFFIEnd(ChiProcessor* CHI_UNUSED(proc), countData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void countInsnEnd(ChiProcessor* CHI_UNUSED(proc), countData* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void countSetup(CbyInterpreter* CHI_UNUSED(interp), countData* CHI_UNUSED(global)) {}
CHI_INL void countDestroy(CbyInterpreter* CHI_UNUSED(interp), countData* CHI_UNUSED(global)) {}
CHI_INL void countProcStart(ChiProcessor* CHI_UNUSED(proc), countData* CHI_UNUSED(local), countData* CHI_UNUSED(global)) {}

CHI_INL void countInsnBegin(ChiProcessor* CHI_UNUSED(proc), countData* local, const CbyCode* CHI_UNUSED(ip), Opcode op) {
    ++local->insn[op];
}

CHI_INL void countEnter(ChiProcessor* CHI_UNUSED(proc), countData* local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), bool jmp) {
    if (jmp)
        ++local->enterJmp;
    else
        ++local->enter;
}

CHI_INL void countLeave(ChiProcessor* CHI_UNUSED(proc), countData* local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {
    ++local->leave;
}

CHI_INL void countProcStop(ChiProcessor* CHI_UNUSED(proc), countData* local, countData* global) {
    // This assertion does not hold unfortunately due to noreturn ffi calls like chiExit
    //CHI_ASSERT(local->enter + local->enterJmp == local->leave);
    global->enter    += local->enter;
    global->leave    += local->leave;
    global->enterJmp += local->enterJmp;
    for (size_t i = 0; i < OPCODE_COUNT; ++i)
        global->insn[i] += local->insn[i];
}
