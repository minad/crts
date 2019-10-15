#pragma once

#include "../bytecode/opcodes.h"
#include "../cby.h"
#include "native/cycles.h"

typedef struct {
    const CbyCode* ffi;
    uint64_t      count, cycles;
} FFICallRecord;

#define HT_HASH   FFICallHash
#define HT_ENTRY  FFICallRecord
#define HT_PREFIX fficallHash
#define HT_KEY(e) e->ffi
#include "native/generic/hashtable.h"

typedef struct {
    FFICallRecord *current;
    uint64_t       cyclesBegin;
    FFICallHash    hash;
} fficallData;

CHI_INL FFICallRecord* fficallRecord(fficallData* data, const CbyCode* ffi) {
    FFICallRecord* r;
    if (!fficallHashCreate(&data->hash, ffi, &r))
        return r;
    r->ffi = ffi;
    return r;
}

CHI_INL void fficallAlloc(ChiProcessor* CHI_UNUSED(proc), fficallData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void fficallEnter(ChiProcessor* CHI_UNUSED(proc), fficallData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), bool CHI_UNUSED(jmp)) {}
CHI_INL void fficallLeave(ChiProcessor* CHI_UNUSED(proc), fficallData* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {}
CHI_INL void fficallInsnBegin(ChiProcessor* CHI_UNUSED(proc), fficallData* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op)) {}
CHI_INL void fficallInsnEnd(ChiProcessor* CHI_UNUSED(proc), fficallData* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void fficallSetup(CbyInterpreter* CHI_UNUSED(interp), fficallData* CHI_UNUSED(global)) {}
CHI_INL void fficallProcStart(ChiProcessor* CHI_UNUSED(proc), fficallData* CHI_UNUSED(local), fficallData* CHI_UNUSED(global)) {}

CHI_INL void fficallFFIBegin(ChiProcessor* CHI_UNUSED(proc), fficallData* local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* ffi) {
    local->current = fficallRecord(local, ffi);
    local->cyclesBegin = chiCpuCycles();
}

CHI_INL void fficallFFIEnd(ChiProcessor* CHI_UNUSED(proc), fficallData* local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {
    ++local->current->count;
    local->current->cycles += chiCpuCycles() - local->cyclesBegin;
}

CHI_INL void fficallFFITail(ChiProcessor* CHI_UNUSED(proc), fficallData* local, Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* ffi) {
    ++fficallRecord(local, ffi)->count;
}

CHI_INL void fficallProcStop(ChiProcessor* CHI_UNUSED(proc), fficallData* local, fficallData* global) {
    CHI_HT_FOREACH(FFICallHash, r, &local->hash) {
        FFICallRecord* t = fficallRecord(global, r->ffi);
        t->count += r->count;
        t->cycles += r->cycles;
    }
    fficallHashFree(&local->hash);
}

CHI_INL void fficallDestroy(CbyInterpreter* CHI_UNUSED(interp), fficallData* global) {
    fficallHashFree(&global->hash);
}
