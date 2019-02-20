#include "cby.h"
#include "stack.h"
#include "location.h"
#include "bytecode/decode.h"

void cbyReadLocation(const CbyCode* code, const CbyCode* fnip, ChiLocInfo* loc) {
    const CbyCode* IP = fnip - CBY_FNHEADER;
    CHI_CLEAR(loc);
    loc->interp = true;
    loc->line = FETCH32;
    loc->file = FETCH_STRING;
    loc->fn = FETCH_STRING;
    loc->size = FETCH32;
    IP = code + CBY_MAGIC_SIZE;
    uint32_t constSize = FETCH32;
    IP += constSize;
    loc->module = FETCH_STRING;
}

void cbyReadFnLocation(Chili fn, ChiLocInfo* loc) {
    CbyFn* f = chiToCbyFn(fn);
    cbyReadLocation(cbyInterpModuleCode(f->module), chiToIP(f->ip), loc);
}

static ChiLoc locateFn(Chili fn) {
    CbyFn* f = chiToCbyFn(fn);
    return (ChiLoc){ .interp = { .code = cbyInterpModuleCode(f->module), .ip = chiToIP(f->ip) } };
}

ChiLoc chiLocateFrame(const Chili* p) {
    if (chiFrame(p) == CHI_FRAME_INTERP)
        return locateFn(p[-3]);
    return chiLocateFrameDefault(p);
}

ChiLoc chiLocateFn(Chili c) {
    if (chiContInfo(chiToCont(CHI_IDX(c, 0)))->type == CHI_FRAME_INTERPFN)
        return locateFn(c);
    return chiLocateFnDefault(c);
}

void chiLocLookup(ChiLocLookup* lookup, ChiLoc loc) {
    if (loc.type != CHI_LOC_NATIVE)
        cbyReadLocation(loc.interp.code, loc.interp.ip, &lookup->loc);
    else
        chiLocLookupDefault(lookup, loc);
}
