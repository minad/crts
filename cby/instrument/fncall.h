#pragma once

#include "../bytecode/opcodes.h"
#include "native/cycles.h"

typedef struct  {
    const CbyCode *code, *ip;
    uint64_t      count, cycles;
} FnCallRecord;

#define HT_HASH      FnCallHash
#define HT_ENTRY     FnCallRecord
#define HT_PREFIX    fncallHash
#define HT_KEY(e)    e->ip
#include "native/generic/hashtable.h"

typedef struct {
    FnCallRecord *current;
    uint64_t      cyclesBegin;
    FnCallHash    hash;
} fncallData;

static CHI_NOINL FnCallRecord* fncallRecord(fncallData* data, const CbyCode* code, const CbyCode* ip) {
    FnCallRecord* r;
    if (!fncallHashCreate(&data->hash, ip, &r))
        return r;
    r->ip = ip;
    r->code = code;
    return r;
}

CHI_INL void fncallAlloc(ChiProcessor* CHI_UNUSED(proc), fncallData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void fncallFFITail(ChiProcessor* CHI_UNUSED(proc), fncallData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void fncallFFIBegin(ChiProcessor* CHI_UNUSED(proc), fncallData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void fncallFFIEnd(ChiProcessor* CHI_UNUSED(proc), fncallData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void fncallInsnBegin(ChiProcessor* CHI_UNUSED(proc), fncallData* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op)) {}
CHI_INL void fncallInsnEnd(ChiProcessor* CHI_UNUSED(proc), fncallData* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void fncallSetup(CbyInterpreter* CHI_UNUSED(interp), fncallData* CHI_UNUSED(global)) {}
CHI_INL void fncallProcStart(ChiProcessor* CHI_UNUSED(proc), fncallData* CHI_UNUSED(local), fncallData* CHI_UNUSED(global)) {}

CHI_INL void fncallEnter(ChiProcessor* CHI_UNUSED(proc), fncallData* local, Chili fn, Chili* CHI_UNUSED(frame), bool CHI_UNUSED(jmp)) {
    CbyFn* f = chiToCbyFn(fn);
    local->current = fncallRecord(local,
                                  cbyInterpModuleCode(f->module),
                                  chiToIP(f->ip));
    local->cyclesBegin = chiCpuCycles();
}

CHI_INL void fncallLeave(ChiProcessor* CHI_UNUSED(proc), fncallData* local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {
   ++local->current->count;
    local->current->cycles += chiCpuCycles() - local->cyclesBegin;
}

CHI_INL void fncallProcStop(ChiProcessor* CHI_UNUSED(proc), fncallData* local, fncallData* global) {
    CHI_HT_FOREACH(FnCallHash, r, &local->hash) {
        FnCallRecord* t = fncallRecord(global, r->code, r->ip);
        t->count += r->count;
        t->cycles += r->cycles;
    }
    fncallHashFree(&local->hash);
}

CHI_INL void fncallDestroy(CbyInterpreter* CHI_UNUSED(interp), fncallData* global) {
    fncallHashFree(&global->hash);
}
