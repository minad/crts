#pragma once

#include "../bytecode/opcodes.h"
#include "../cby.h"
#include "native/tracepoint.h"

CHI_UNITTYPE(profData)

void _profTrace(ChiProcessor*, Chili, Chili*, const CbyCode*, size_t);
void _profSetup(CbyInterpreter*);

CHI_INL void profTime(ChiProcessor* proc, Chili fn, Chili* frame, const CbyCode* ffi) {
    if (chiTraceTimeTriggered(proc->worker))
        _profTrace(proc, fn, frame, ffi, 0);
}

CHI_INL void profAlloc(ChiProcessor* proc, profData* CHI_UNUSED(local), Chili fn, Chili* frame, size_t size) {
    if (chiTraceAllocTriggered(proc->worker))
        _profTrace(proc, fn, frame, 0, size);
}

CHI_INL void profDestroy(CbyInterpreter* CHI_UNUSED(interp), profData* CHI_UNUSED(global))   {}
CHI_INL void profEnter(ChiProcessor* proc, profData* CHI_UNUSED(local), Chili fn, Chili* frame, bool CHI_UNUSED(jmp))  { profTime(proc, fn, frame, 0);   }
CHI_INL void profFFIBegin(ChiProcessor* proc, profData* CHI_UNUSED(local), Chili fn, Chili* frame, const CbyCode* ffi) { profTime(proc, fn, frame, ffi); }
CHI_INL void profFFIEnd(ChiProcessor* proc, profData* CHI_UNUSED(local), Chili fn, Chili* frame, const CbyCode* ffi)   { profTime(proc, fn, frame, ffi); }
CHI_INL void profFFITail(ChiProcessor* CHI_UNUSED(proc), profData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void profInsnBegin(ChiProcessor* CHI_UNUSED(proc), profData* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op))         {}
CHI_INL void profInsnEnd(ChiProcessor* CHI_UNUSED(proc), profData* CHI_UNUSED(local), Opcode CHI_UNUSED(op))           {}
CHI_INL void profLeave(ChiProcessor* proc, profData* CHI_UNUSED(local), Chili fn, Chili* frame)                       { profTime(proc, fn, frame, 0);   }
CHI_INL void profProcStart(ChiProcessor* CHI_UNUSED(proc), profData* CHI_UNUSED(local), profData* CHI_UNUSED(data))       {}
CHI_INL void profProcStop(ChiProcessor* CHI_UNUSED(proc), profData* CHI_UNUSED(local), profData* CHI_UNUSED(global))   {}
CHI_INL void profSetup(CbyInterpreter* interp, profData* CHI_UNUSED(global)) { _profSetup(interp); }
