#include "bytecode/decode.h"
#include "cby.h"
#include "native/location.h"
#include "native/stack.h"

void cbyReadLocation(const CbyCode* fnip, ChiLocInfo* loc) {
    const CbyCode* IP = fnip - CBY_FNHEADER;
    CHI_ZERO_STRUCT(loc);
    loc->interp = true;
    loc->line = FETCH32;
    loc->file = FETCH_STRING;
    loc->fn = FETCH_STRING;
    loc->size = FETCH32;
}

void cbyReadFnLocation(Chili fn, ChiLocInfo* loc) {
    CbyFn* f = chiToCbyFn(fn);
    cbyReadLocation(chiToIP(f->ip), loc);
}

ChiLoc chiLocateFrame(const Chili* p) {
    return chiFrameInfo(p)->interp
        ? (ChiLoc){ .type = CHI_LOC_INTERP, .id = chiToIP(chiToCbyFn(p[-3])->ip) }
        : chiLocateFrameDefault(p);
}

void chiLocResolve(ChiLocResolve* resolve, ChiLoc loc, bool mangled) {
    if (loc.type == CHI_LOC_INTERP)
        cbyReadLocation((const CbyCode*)loc.id, &resolve->loc);
    else
        chiLocResolveDefault(resolve, loc, mangled);
}
