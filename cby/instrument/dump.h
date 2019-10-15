#pragma once

#include "../bytecode/opcodes.h"
#include "../cby.h"
#include "native/sink.h"

typedef struct {
    ChiSink*       sink;
    CbyFn*         fn;
    const CbyCode* fnBegin, *codeStart;
    uint64_t       count;
} dumpData;

void _dumpEnter(dumpData*, Chili);
void _dumpInsnBegin(dumpData*, const CbyCode*);
void _dumpProcStart(ChiProcessor*, dumpData*);
void _dumpProcStop(dumpData*);

CHI_INL void dumpAlloc(ChiProcessor* CHI_UNUSED(proc), dumpData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void dumpDestroy(CbyInterpreter* CHI_UNUSED(interp), dumpData* CHI_UNUSED(global)) {}
CHI_INL void dumpEnter(ChiProcessor* CHI_UNUSED(proc), dumpData* local, Chili fn, Chili* CHI_UNUSED(frame), bool CHI_UNUSED(jmp)) { _dumpEnter(local, fn); }
CHI_INL void dumpFFIBegin(ChiProcessor* CHI_UNUSED(proc), dumpData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void dumpFFIEnd(ChiProcessor* CHI_UNUSED(proc), dumpData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void dumpFFITail(ChiProcessor* CHI_UNUSED(proc), dumpData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void dumpInsnBegin(ChiProcessor* CHI_UNUSED(proc), dumpData* local, const CbyCode* ip, Opcode CHI_UNUSED(op)) { _dumpInsnBegin(local, ip); }
CHI_INL void dumpInsnEnd(ChiProcessor* CHI_UNUSED(proc), dumpData* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void dumpLeave(ChiProcessor* CHI_UNUSED(proc), dumpData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {}
CHI_INL void dumpProcStart(ChiProcessor* proc, dumpData* local, dumpData* CHI_UNUSED(global)) { _dumpProcStart(proc, local); }
CHI_INL void dumpProcStop(ChiProcessor* CHI_UNUSED(proc), dumpData* local, dumpData* CHI_UNUSED(global)) { _dumpProcStop(local); }
CHI_INL void dumpSetup(CbyInterpreter* CHI_UNUSED(interp), dumpData* CHI_UNUSED(global)) {}
