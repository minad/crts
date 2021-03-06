#pragma once

#include "../bytecode/opcodes.h"
#include "../cby.h"

CHI_UNITTYPE(nullData)
_Static_assert(sizeof (nullData) == 0, "nullData should have length 0");

CHI_INL void nullAlloc(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void nullEnter(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), bool CHI_UNUSED(jmp)) {}
CHI_INL void nullLeave(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {}
CHI_INL void nullFFITail(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void nullFFIBegin(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void nullFFIEnd(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void nullInsnBegin(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op)) {}
CHI_INL void nullInsnEnd(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void nullSetup(CbyInterpreter* CHI_UNUSED(interp), nullData* CHI_UNUSED(global)) {}
CHI_INL void nullDestroy(CbyInterpreter* CHI_UNUSED(interp), nullData* CHI_UNUSED(global)) {}
CHI_INL void nullProcStart(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), nullData* CHI_UNUSED(global)) {}
CHI_INL void nullProcStop(ChiProcessor* CHI_UNUSED(proc), nullData* CHI_UNUSED(local), nullData* CHI_UNUSED(global)) {}
