#pragma once

#include "asm.h"

typedef struct  {
    const CbyCode *code, *ip;
    uint64_t      count, cycles;
} FnCallRecord;

#define HASH      FnCallHash
#define ENTRY     FnCallRecord
#define PREFIX    fncallHash
#define KEY(e)    e->ip
#include "hashtable.h"

typedef struct {
    FnCallRecord *current;
    uint64_t      cyclesBegin;
    FnCallHash    hash;
} fncallData_t;

static CHI_NOINL FnCallRecord* fncallRecord(fncallData_t* data, const CbyCode* code, const CbyCode* ip) {
    FnCallRecord* r;
    if (!fncallHashCreate(&data->hash, ip, &r))
        return r;
    r->ip = ip;
    r->code = code;
    return r;
}

CHI_INL void fncallAlloc(ChiProcessor* CHI_UNUSED(proc), fncallData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void fncallFFITail(ChiProcessor* CHI_UNUSED(proc), fncallData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void fncallFFIBegin(ChiProcessor* CHI_UNUSED(proc), fncallData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void fncallFFIEnd(ChiProcessor* CHI_UNUSED(proc), fncallData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void fncallInsnBegin(ChiProcessor* CHI_UNUSED(proc), fncallData_t* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op)) {}
CHI_INL void fncallInsnEnd(ChiProcessor* CHI_UNUSED(proc), fncallData_t* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void fncallSetup(CbyInterpreter* CHI_UNUSED(interp), fncallData_t* CHI_UNUSED(global)) {}
CHI_INL void fncallProcStart(ChiProcessor* CHI_UNUSED(proc), fncallData_t* CHI_UNUSED(local), fncallData_t* CHI_UNUSED(global)) {}

CHI_INL void fncallEnter(ChiProcessor* CHI_UNUSED(proc), fncallData_t* local, Chili fn, Chili* CHI_UNUSED(frame), bool CHI_UNUSED(jmp)) {
    CbyFn* f = chiToCbyFn(fn);
    local->current = fncallRecord(local,
                                  cbyInterpModuleCode(f->module),
                                  chiToIP(f->ip));
    local->cyclesBegin = chiCpuCycles();
}

CHI_INL void fncallLeave(ChiProcessor* CHI_UNUSED(proc), fncallData_t* local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {
   ++local->current->count;
    local->current->cycles += chiCpuCycles() - local->cyclesBegin;
}

CHI_INL void fncallProcStop(ChiProcessor* CHI_UNUSED(proc), fncallData_t* local, fncallData_t* global) {
    HASH_FOREACH(fncallHash, r, &local->hash) {
        FnCallRecord* t = fncallRecord(global, r->code, r->ip);
        t->count += r->count;
        t->cycles += r->cycles;
    }
    fncallHashDestroy(&local->hash);
}

CHI_INL void fncallDestroy(CbyInterpreter* CHI_UNUSED(interp), fncallData_t* global) {
    fncallHashDestroy(&global->hash);
}
