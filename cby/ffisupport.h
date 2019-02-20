#pragma once

#include "ffidefs.h"
#include "error.h"

#define _FFI_TYPE_ENUM(name, libffi, dcarg, dccall, type) FFI_##name,
typedef enum { CHI_FOREACH_FFI_TYPE(,_FFI_TYPE_ENUM) } FFIType;

typedef struct { int _void; } ChiVoid;
CHI_INL ChiVoid chiToVoid(Chili CHI_UNUSED(c))     { CHI_BUG("Invalid FFI argument type 'void'"); }
CHI_INL Chili   chiFromVoid(ChiVoid CHI_UNUSED(v)) { return CHI_FALSE; }
CHI_INL Chili   chiToChili(Chili c)                { return c; }
CHI_INL Chili   chiFromChili(Chili c)              { return c; }

CHI_INL void toFFI(void* r, Chili c, FFIType t) {
    switch (t) {
#define _FFI_TO(name, libffi, dcarg, dccall, type) case FFI_##name: *(type*)r = chiTo##name(c); break;
        CHI_FOREACH_FFI_TYPE(,_FFI_TO)
#undef _FFI_TO
    default: CHI_BUG("Invalid FFI argument type"); break;
    }
}

CHI_INL Chili fromFFI(void* a, FFIType t) {
    switch (t) {
#define _FFI_FROM(name, libffi, dcarg, dccall, type) case FFI_##name: return chiFrom##name(*(type*)a); break;
        CHI_FOREACH_FFI_TYPE(,_FFI_FROM)
#undef _FFI_FROM
    default: CHI_BUG("Invalid FFI return type"); return CHI_FALSE;
    }
}

#if CBY_HAS_FFI_BACKEND(libffi)
typedef struct {
    uint64_t argVal[CBY_FFI_AMAX];
    void* arg[CBY_FFI_AMAX];
} FFIData;

CHI_INL void ffiArgs(FFIData* d, const CbyCode* ffiref, const Chili* REG, uint32_t nargs, const CbyCode* IP) {
    for (uint32_t i = 0; i < nargs; ++i)
        toFFI(d->arg[i], REG[chiPeekUInt16(IP + 2 * i)], (FFIType)ffiref[10 + i]);
}

CHI_INL Chili ffiCall(FFIData* d, const CbyCode* ffiref, CbyFFI* ffi) {
    uint64_t ret;
    ffi_call(&ffi->cif, FFI_FN(ffi->fn), &ret, d->arg);
    return fromFFI(&ret, (FFIType)ffiref[8]);
}

CHI_INL void ffiSetup(FFIData* d) {
    for (uint32_t i = 0; i < CBY_FFI_AMAX; ++i)
        d->arg[i] = d->argVal + i;
}

CHI_INL void ffiDestroy(FFIData* CHI_UNUSED(d)) {
}
#elif CBY_HAS_FFI_BACKEND(dyncall)
#include <dyncall.h>

typedef struct {
    DCCallVM* dc;
} FFIData;

CHI_INL void ffiArg(DCCallVM* v, FFIType t, Chili c) {
    switch (t) {
#define _FFI_TO(name, libffi, dcarg, dccall, type) case FFI_##name: { type f = chiTo##name(c); dcarg; break; }
        CHI_FOREACH_FFI_TYPE(,_FFI_TO)
#undef _FFI_TO
    default: CHI_BUG("Invalid FFI argument type"); break;
    }
}

CHI_INL void ffiArgs(FFIData* d, const CbyCode* ffiref, const Chili* REG, uint32_t nargs, const CbyCode* IP) {
    dcReset(d->dc);
    for (uint32_t i = 0; i < nargs; ++i)
        ffiArg(d->dc, (FFIType)ffiref[10 + i], REG[chiPeekUInt16(IP + 2 * i)]);
}

CHI_INL Chili ffiCall(FFIData* d, const CbyCode* ffiref, CbyFFI* ffi) {
    DCCallVM* v = d->dc;
    DCpointer f = ffi->fn;
    switch ((FFIType)ffiref[8]) {
#define _FFI_FROM(name, libffi, dcarg, dccall, type) case FFI_##name: return chiFrom##name(dccall); break;
        CHI_FOREACH_FFI_TYPE(,_FFI_FROM)
#undef _FFI_FROM
    default: CHI_BUG("Invalid FFI return type"); return CHI_FALSE;
    }
}

CHI_INL void ffiSetup(FFIData* d) {
    if (!(d->dc = dcNewCallVM(CBY_FFI_AMAX * sizeof (uint64_t))))
        chiErr("dcNewCallVM failed");
}

CHI_INL void ffiDestroy(FFIData* d) {
    dcFree(d->dc);
}
#else
typedef int FFIData[0];
CHI_INL void ffiArgs(FFIData* CHI_UNUSED(d), const CbyCode* CHI_UNUSED(ffiref), const Chili* CHI_UNUSED(REG), uint32_t CHI_UNUSED(nargs), const CbyCode* CHI_UNUSED(IP)) {}
CHI_INL Chili ffiCall(FFIData* CHI_UNUSED(d), const CbyCode* CHI_UNUSED(ffiref), CbyFFI* CHI_UNUSED(ffi)) { return CHI_FALSE; }
CHI_INL void ffiSetup(FFIData* CHI_UNUSED(d)) {}
CHI_INL void ffiDestroy(FFIData* CHI_UNUSED(d)) {}
#endif
