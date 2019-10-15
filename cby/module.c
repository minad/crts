#include <chili/cont.h>
#include "bytecode/decode.h"
#include "cby.h"
#include "native/exception.h"
#include "native/export.h"
#include "native/location.h"
#include "native/new.h"
#include "native/sink.h"
#include "native/stack.h"

struct ChiModuleEntry_ {
    Chili pair;
};

#define HT_NOSTRUCT
#define HT_HASH        ChiModuleHash
#define HT_ENTRY       ChiModuleEntry
#define HT_PREFIX      moduleHash
#define HT_KEY(e)      chiStringRef(&chiToCbyModule(chiIdx(e->pair, 1))->name)
#define HT_EXISTS(e)   chiTrue(e->pair)
#define HT_KEYEQ(a, b) chiStringRefEq(a, b)
#define HT_HASHFN      chiHashStringRef
#include "native/generic/hashtable.h"

int32_t
    z_System__EntryPoints_z_start,
    z_System__EntryPoints_z_blackhole,
    z_System__EntryPoints_z_unhandled,
    z_System__EntryPoints_z_userInterrupt,
    z_System__EntryPoints_z_timerInterrupt,
    z_Main_z_main;

static Chili findModule(ChiRuntime* rt, ChiStringRef name) {
    ChiModuleEntry* mod = moduleHashFind(&rt->moduleHash, name);
    return mod ? chiIdx(mod->pair, 0) : CHI_FALSE;
}

CHI_STATIC_CONT_DECL(loadModule)
CHI_STATIC_CONT_DECL(interpInitImport)

static ChiLoc locateInit(Chili mod) {
    if (chiType(mod) == CBY_NATIVE_MODULE)
        return (ChiLoc){ .type = CHI_LOC_NATIVE, .cont = chiToCont(chiToCbyNativeModule(mod)->base.init) };
    const CbyCode* IP = chiToIP(chiToCbyInterpModule(mod)->base.init);
    uint32_t initOffset = FETCH32;
    return (ChiLoc){ .type = CHI_LOC_INTERP, .id = IP + initOffset };
}

STATIC_CONT(interpInitModuleCont, .size = 2) {
    PROLOGUE(interpInitModuleCont);
    SP -= 2;
    Chili mod = SP[0];
    Chili value = A(0);
    ChiStringRef name = chiStringRef(&chiToCbyModule(mod)->name);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    Chili pair = chiNewTupleFlags(proc, CHI_NEW_LOCAL, value, mod);
    moduleHashInsert(&proc->rt->moduleHash, moduleHashFn(name))->pair = pair;
    chiGCRoot(proc->rt, pair);
    if (chiEventEnabled(proc, MODULE_INIT)) {
        ChiLocResolve resolve;
        chiLocResolve(&resolve, locateInit(mod), proc->rt->option.stack.traceMangled);
        chiEventStruct(proc, MODULE_INIT, &resolve.loc);
    }
    RET(value);
}

STATIC_CONT(interpInitImportCont, .size = 3) {
    PROLOGUE(interpInitImportCont);
    SP -= 3;
    Chili ret = A(0);
    uint32_t nimports = chiToUInt32(SP[0]);
    uint32_t idx = chiToUInt32(SP[1]);
    SP[(int32_t)idx - 1 - (int32_t)chiFrameSize(SP - 1)] = ret;
    A(0) = chiFromUInt32(nimports);
    A(1) = chiFromUInt32(idx);
    KNOWN_JUMP(interpInitImport);
}

STATIC_CONT(interpInitImport, .na = 1) {
    PROLOGUE(interpInitImport);
    LIMITS(.stack = 3);

    uint32_t nimports = chiToUInt32(A(0));
    uint32_t idx = chiToUInt32(A(1));

    if (idx == nimports)
        RET(CHI_FALSE);

    SP[0] = chiFromUInt32(nimports);
    SP[1] = chiFromUInt32(idx + 1);
    SP[2] = chiFromCont(&interpInitImportCont);
    SP += 3;

    A(0) = SP[(int32_t)idx - 3 - (int32_t)chiFrameSize(SP - 3 - 1)];
    KNOWN_JUMP(loadModule);
}

CONT(cbyModuleInit) {
    PROLOGUE(cbyModuleInit);

    Chili mod = A(0);
    CbyModule* m = chiToCbyModule(mod);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    Chili loaded = findModule(proc->rt, chiStringRef(&m->name));
    if (chiTrue(loaded))
        RET(loaded);

    uint32_t nimports = (uint32_t)chiSize(m->imports);
    LIMITS(.stack = 2 + nimports + CBY_CONTEXT_SIZE, .heap = CHI_SIZEOF_WORDS(CbyFn));

    SP[0] = mod;
    SP[1] = chiFromCont(&interpInitModuleCont);
    SP += 2;

    for (uint32_t i = 0; i < nimports; ++i)
        SP[i] = chiArrayRead(m->imports, i);
    SP += nimports;

    if (chiType(mod) == CBY_NATIVE_MODULE) {
        *SP++ = m->init;
        CHI_ASSERT(chiFrameSize(SP - 1) == nimports + 1);
    } else {
        const CbyCode* IP = chiToIP(m->init);
        uint32_t initOffset = FETCH32;
        Chili init = chiFromIP(IP + initOffset);
        CbyInterpreter* interp = cbyInterpreter(proc);
        if (interp->backend.impl->rewrite) {
            uint32_t codeSize = FETCH32;
            interp->backend.impl->rewrite(CHI_CONST_CAST(IP, CbyCode*), IP + codeSize);
        }

        NEW(fn, CHI_FIRST_FN, CHI_SIZEOF_WORDS(CbyFn));
        CbyFn* f = NEW_PAYLOAD(CbyFn, fn);
        f->fn = CHI_FALSE;
        f->module = mod;
        f->ip = init;

        SP[0] = fn;
        SP[1] = init;
        SP[2] = chiFromUnboxed(nimports + CBY_CONTEXT_SIZE);
        SP[3] = chiFromCont(interp->backend.impl->cont);
        SP += CBY_CONTEXT_SIZE;
    }

    A(0) = chiFromUInt32(nimports);
    A(1) = CHI_FALSE;
    KNOWN_JUMP(interpInitImport);
}

STATIC_CONT(loadModule) {
    PROLOGUE(loadModule);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiStringRef name = chiStringRef(&A(0));
    Chili loaded = findModule(proc->rt, name);
    if (chiTrue(loaded))
        RET(loaded);

    Chili mod = cbyModuleLoadByName(proc, name);
    if (!chiEither(&mod))
        CHI_THROW_RUNTIME_EX(proc, mod);

    A(0) = mod;
    KNOWN_JUMP(cbyModuleInit);
}

STATIC_CONT(interpreterMain) {
    PROLOGUE(interpreterMain);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;
    const char* name = rt->argv[1];
    --rt->argc;
    memmove(rt->argv + 1, rt->argv + 2, sizeof (char*) * rt->argc);
    Chili mod = cbyModuleLoadByNameOrFile(proc, name);
    if (!chiEither(&mod))
        CHI_THROW_RUNTIME_EX(proc, mod);

    z_Main_z_main = cbyModuleExport(mod, "main");

    A(0) = mod;
    KNOWN_JUMP(cbyModuleInit);
}

static void disasm(ChiProcessor* proc) {
    ChiRuntime* rt = proc->rt;
    CHI_AUTO_SINK(sink, chiSinkColor(CHI_AND(CHI_COLOR_ENABLED, rt->option.color)));
    chiPager();
    int32_t res = CHI_EXIT_SUCCESS;
    for (uint32_t i = 1; i < rt->argc; ++i) {
        Chili mod = cbyModuleLoadByNameOrFile(proc, rt->argv[i]);
        if (!chiEither(&mod)) {
            chiSinkFmt(sink, "%S\n", chiStringRef(&mod));
            res = CHI_EXIT_ERROR;
            break;
        }
        if (chiType(mod) != CBY_INTERP_MODULE) {
            chiSinkFmt(sink, "Cannot disassemble native module '%s'", rt->argv[i]);
            res = CHI_EXIT_ERROR;
                break;
        }
        Chili code = chiToCbyInterpModule(mod)->code;
        if (!cbyDisasm(sink, cbyCode(code), cbyCodeEnd(code))) {
            res = CHI_EXIT_ERROR;
            break;
        }
    }
    rt->exitHandler(res);
}

CONT(z_Main) {
    PROLOGUE(z_Main);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (CHI_AND(CBY_DISASM_ENABLED, cbyInterpreter(proc)->option.disasm))
        disasm(proc);

    KNOWN_JUMP(interpreterMain);
}

#define LOAD_FUNCTION(mod, name, fn)                                    \
    ({                                                                  \
        int32_t idx = cbyModuleExport((mod), (fn));                    \
        if (idx < 0)                                                    \
            CHI_THROW_RUNTIME_EX(proc,                                  \
                                 chiFmtString(proc, "Could not load exported function '%s' from '%S'.", \
                                               (fn), (name)));          \
        idx;                                                            \
    })

CONT(z_System__EntryPoints) {
    PROLOGUE(z_System__EntryPoints);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiStringRef name = CHI_STRINGREF("System/EntryPoints");
    Chili loaded = findModule(proc->rt, name);
    if (chiTrue(loaded))
        RET(loaded);

    Chili mod = cbyModuleLoadByName(proc, name);
    if (!chiEither(&mod))
        RET(CHI_FAIL);

    z_System__EntryPoints_z_start = LOAD_FUNCTION(mod, name, "start");
    z_System__EntryPoints_z_blackhole = LOAD_FUNCTION(mod, name, "blackhole");
    z_System__EntryPoints_z_unhandled = LOAD_FUNCTION(mod, name, "unhandled");
    z_System__EntryPoints_z_timerInterrupt = LOAD_FUNCTION(mod, name, "timerInterrupt");
    z_System__EntryPoints_z_userInterrupt = LOAD_FUNCTION(mod, name, "userInterrupt");

    A(0) = mod;
    KNOWN_JUMP(cbyModuleInit);
}

CONT(cbyFFIError) {
    PROLOGUE(cbyFFIError);
    const CbyCode* IP = chiToIP(A(0));
    ChiStringRef name = FETCH_STRING;
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_THROW_RUNTIME_EX(proc, chiFmtString(proc, "FFI function '%S' not loaded", name));
}

void cbyModuleUnload(Chili name) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiStringRef nameRef = chiStringRef(&name);
    ChiModuleEntry* entry = moduleHashFind(&proc->rt->moduleHash, nameRef);
    if (entry) {
        chiGCUnroot(proc->rt, entry->pair);
        CHI_IGNORE_RESULT(moduleHashDelete(&proc->rt->moduleHash, nameRef));
        chiEvent(proc, MODULE_UNLOAD, .module = nameRef);
    }
}
