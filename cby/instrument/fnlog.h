#pragma once

#include "../bytecode/opcodes.h"
#include "../cby.h"

void _fnlogEnter(ChiProcessor*, Chili, bool);
void _fnlogLeave(ChiProcessor*, Chili);
void _fnlogFFI(ChiProcessor*, const CbyCode*);

CHI_UNITTYPE(fnlogData)
CHI_INL void fnlogAlloc(ChiProcessor* CHI_UNUSED(proc), fnlogData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void fnlogDestroy(CbyInterpreter* CHI_UNUSED(interp), fnlogData* CHI_UNUSED(global)) {}
CHI_INL void fnlogEnter(ChiProcessor* proc, fnlogData* CHI_UNUSED(local), Chili fn, Chili* CHI_UNUSED(frame), bool jmp) { _fnlogEnter(proc, fn, jmp); }
CHI_INL void fnlogFFIBegin(ChiProcessor* proc, fnlogData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* ffi) { _fnlogFFI(proc, ffi); }
CHI_INL void fnlogFFIEnd(ChiProcessor* CHI_UNUSED(proc), fnlogData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void fnlogFFITail(ChiProcessor* proc, fnlogData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* ffi) { _fnlogFFI(proc, ffi); }
CHI_INL void fnlogInsnBegin(ChiProcessor* CHI_UNUSED(proc), fnlogData* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op)) {}
CHI_INL void fnlogInsnEnd(ChiProcessor* CHI_UNUSED(proc), fnlogData* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void fnlogLeave(ChiProcessor* proc, fnlogData* CHI_UNUSED(local), Chili fn, Chili* CHI_UNUSED(frame)) { _fnlogLeave(proc, fn); }
CHI_INL void fnlogProcStart(ChiProcessor* CHI_UNUSED(proc), fnlogData* CHI_UNUSED(local), fnlogData* CHI_UNUSED(global)) {}
CHI_INL void fnlogProcStop(ChiProcessor* CHI_UNUSED(proc), fnlogData* CHI_UNUSED(local), fnlogData* CHI_UNUSED(global)) {}
CHI_INL void fnlogSetup(CbyInterpreter* CHI_UNUSED(interp), fnlogData* CHI_UNUSED(global)) {}
