#pragma once

#include "runtime.h"
#include "stats.h"
#include "profiler.h"
#include <chili/object.h>

typedef struct CbyDyn_ CbyDyn;
typedef struct ChiSink_ ChiSink;
typedef uint8_t CbyCode;
typedef void (*CbyRewrite)(CbyCode*, const CbyCode*);
typedef struct CbyInterpreter_ CbyInterpreter;
typedef void (*CbySetup)(CbyInterpreter*);

enum {
      CBY_FNHEADER = 16,
      CBY_MAGIC_SIZE = 4,
};

/**
 * Chili object used for interpreted functions. The format
 * of the object must be compatible with the normal runtime
 * functions chiApp etc. The first field holds a pointer
 * to the continuation associated with the function.
 * Here this pointer points to the interpreter.
 */
typedef struct CHI_PACKED CHI_WORD_ALIGNED {
    Chili fn, module, ip, clos[];
} CbyFn;

/**
 * Interpreter backend data. Multiple backends
 * are supported, providing different types
 * of instrumentation, e.g., instruction counting and profiling.
 * The backends are switchable to avoid a performance impact
 * in the uninstrumented interpreter.
 */
typedef const struct {
    ChiCont    cont;
    CbyRewrite rewrite;
    CbySetup   setup;
    ChiHookFn  procStart, procStop;
} CbyBackend;

/**
 * Module loader
 */
typedef struct {
    char* path;
    void* archive;
    char* archiveName;
} CbyModuleLoader;

/**
 * Module object loaded from bytecode
 */
typedef struct {
    Chili name, init, exports, imports;
} CbyModule;

/**
 * Interpreter specific options
 */
typedef struct {
    ChiProfOption  prof[CBY_BACKEND_PROF_ENABLED];
    bool repl;
    CHI_IF(CBY_DISASM_ENABLED, bool disasm;)
    CHI_IF(CBY_LSMOD_ENABLED, bool lsmod;)
    CHI_IF(CBY_BACKEND_DUMP_ENABLED, bool dumpFile;)
} CbyOption;

/**
 * Main interpreter datastructure, which
 * also holds the ChiRuntime.
 */
struct CbyInterpreter_ {
    ChiRuntime            rt;
    CbyModuleLoader       ml;
    struct {
        void*             state;
        ChiMutex          mutex;
        uint32_t          id;
        const CbyBackend* impl;
    } backend;
    CbyOption             option;
};

typedef struct ChiLocInfo_ ChiLocInfo;

#define CBY_OBJECT(NAME, Name, ...) _CHI_OBJECT(CBY##_##NAME, Cby##Name, Cby##Name, ##__VA_ARGS__)

// We keep the handle to the shared object
// however finalizers and module unloading is not supported right now
CBY_OBJECT(NATIVE_MODULE, NativeModule, Chili name, init, exports, imports, handle)
CBY_OBJECT(INTERP_MODULE, InterpModule, Chili name, init, exports, imports, code, ffi[])

#define _CBY_FFI_BACKEND_none    0
#define _CBY_FFI_BACKEND_libffi  1
#define _CBY_FFI_BACKEND_dyncall 2
#define CBY_HAS_FFI_BACKEND(x)  (CHI_CAT(_CBY_FFI_BACKEND_, x) == CHI_CAT(_CBY_FFI_BACKEND_, CBY_FFI_BACKEND))

#if CBY_HAS_FFI_BACKEND(libffi)
#include <ffi.h>
CBY_OBJECT(FFI, FFI, void *fn; CbyDyn *handle; ffi_cif cif; ffi_type* atype[])
#else
CBY_OBJECT(FFI, FFI, void *fn; CbyDyn *handle)
#endif

CHI_WU int32_t cbyModuleExport(Chili, const char*);
CHI_WU Chili cbyLoadByName(CbyModuleLoader*, ChiStringRef);
CHI_WU Chili cbyLoadByNameOrFile(CbyModuleLoader*, const char*);
CHI_WU Chili cbyLoadFromFile(CbyModuleLoader*, const char*);
CHI_WU Chili cbyLoadFromStdin(CbyModuleLoader*, uint32_t);
void cbyAddPath(CbyModuleLoader*, ChiStringRef);
void cbyListModules(CbyModuleLoader*, ChiSink*);
void cbyModuleLoaderDestroy(CbyModuleLoader*);
void cbyReadLocation(const CbyCode*, const CbyCode*, ChiLocInfo*);
void cbyReadFnLocation(Chili, ChiLocInfo*);
CHI_WU bool cbyDisasm(ChiSink*, const CbyCode*, const CbyCode*);
CHI_WU const CbyCode* cbyDisasmInsn(ChiSink*, const CbyCode*, const CbyCode*, const CbyCode*, const CbyCode*);
void cbyDirectDispatchInit(const char*, void**, void**, void*);
void cbyDirectDispatchRewrite(void**, CbyCode*, const CbyCode*);
CHI_WU bool cbyLibrary(const char*);

CHI_INL CHI_WU CbyInterpreter* cbyInterpreter(ChiProcessor* proc) {
    return CHI_OUTER(CbyInterpreter, rt, proc->rt);
}

CHI_INL CHI_WU CbyModule* chiToCbyModule(Chili c) {
    CHI_ASSERT(chiType(c) == CBY_NATIVE_MODULE || chiType(c) == CBY_INTERP_MODULE);
    return (CbyModule*)chiRawPayload(c);
}

CHI_INL CbyCode* cbyCode(Chili c) {
    return chiBufferBytes(c);
}

CHI_INL const CbyCode* cbyCodeEnd(Chili c) {
    return chiBufferBytes(c) + chiBufferSize(c);
}

CHI_INL CbyCode* cbyInterpModuleCode(Chili c) {
    return cbyCode(chiToCbyInterpModule(c)->code);
}

CHI_INL CbyFn* chiToCbyFn(Chili c) {
    CHI_ASSERT(chiFn(chiType(c)) || chiType(c) == CHI_THUNKFN);
    return (CbyFn*)chiRawPayload(c);
}

CHI_INL size_t cbyFnClos(Chili c) {
    return chiSize(c) - CHI_SIZEOF_WORDS(CbyFn);
}

CHI_INL CHI_WU Chili chiFromIP(const CbyCode* p) {
    return chiFromPtr(p);
}

CHI_INL CHI_WU const CbyCode* chiToIP(Chili c) {
    return (const CbyCode*)chiToPtr(c);
}

extern const char* const cbyOpName[];
extern const char* const cbyFFIType[];
