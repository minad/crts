#include <chili/cont.h>
#include "bytecode/decode.h"
#include "cby.h"
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

extern int32_t main_z_Main;
int32_t main_z_Main;

static Chili findModule(ChiRuntime* rt, ChiStringRef name) {
    ChiModuleEntry* mod = moduleHashFind(&rt->moduleHash, name);
    return mod ? chiIdx(mod->pair, 0) : CHI_FALSE;
}

CHI_STATIC_CONT_DECL(loadModule)

static ChiLoc locateInit(Chili mod) {
    if (chiType(mod) == CBY_NATIVE_MODULE)
        return (ChiLoc){ .type = CHI_LOC_NATIVE, .cont = chiToCont(chiToCbyNativeModule(mod)->base.init) };
    const CbyCode* IP = chiToIP(chiToCbyInterpModule(mod)->base.init);
    uint32_t initOffset = FETCH32;
    return (ChiLoc){ .type = CHI_LOC_INTERP, .id = IP + initOffset };
}

STATIC_CONT(interpInitModuleCont, .size = 2, .na = 1) {
    PROLOGUE(interpInitModuleCont);
    Chili value = AGET(0);
    UNDEF_ARGS(0);
    SP -= 2;
    Chili mod = SP[0];
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
    ASET(0, value);
    RET;
}

STATIC_CONT(interpInitImportCont, .size = CHI_VASIZE, .na = 1) {
    PROLOGUE(interpInitImportCont);
    Chili ret = AGET(0);
    UNDEF_ARGS(0);
    uint32_t pos = chiToUInt32(SP[-2]), idx = chiToUInt32(SP[-3]);
    SP[(int32_t)idx - (int32_t)pos] = ret;

    if (!idx) {
        SP -= 3;
        RET;
    }
    --idx;

    SP[-3] = chiFromUInt32(idx);
    ASET(0, SP[(int32_t)idx - (int32_t)pos]);
    KNOWN_JUMP(loadModule);
}

CONT(cbyModuleInit, .na = 1) {
    PROLOGUE(cbyModuleInit);

    Chili mod = AGET(0);
    CbyModule* m = chiToCbyModule(mod);
    uint32_t nimports = (uint32_t)chiSize(m->imports);
    LIMITS_PROC(.stack = 2 + // interpInitModuleCont
                nimports + CBY_CONTEXT_SIZE + // imports + module init continuation
                3, // interpInitImport
                .heap = CHI_SIZEOF_WORDS(CbyFn));

    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    Chili loaded = findModule(proc->rt, chiStringRef(&m->name));
    if (chiTrue(loaded)) {
        ASET(0, loaded);
        RET;
    }

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
        f->cont = f->val = CHI_FALSE;
        f->module = mod;
        f->ip = init;

        SP[0] = fn;
        SP[1] = init;
        SP[2] = chiFromUnboxed(nimports + CBY_CONTEXT_SIZE);
        SP[3] = chiFromCont(interp->backend.impl->cont);
        SP += CBY_CONTEXT_SIZE;
    }

    if (!nimports)
        RET;

    SP[0] = chiFromUInt32(nimports - 1);
    SP[1] = chiFromUInt32(3 + chiFrameSize(SP - 1));
    SP[2] = chiFromCont(&interpInitImportCont);
    SP += 3;

    chiStackDebugWalk(chiThreadStack(proc->thread), SP, SL);

    ASET(0, chiArrayRead(m->imports, nimports - 1));
    KNOWN_JUMP(loadModule);
}

STATIC_CONT(loadModule, .na = 1) {
    PROLOGUE(loadModule);
    Chili name = AGET(0);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiStringRef nameRef = chiStringRef(&name);
    Chili loaded = findModule(proc->rt, nameRef);
    if (chiTrue(loaded)) {
        ASET(0, loaded);
        RET;
    }

    Chili mod = cbyModuleLoadByName(proc, nameRef);
    if (!chiEither(&mod)) {
        ASET(0, mod);
        KNOWN_JUMP(chiThrowRuntimeException);
    }

    ASET(0, mod);
    KNOWN_JUMP(cbyModuleInit);
}

STATIC_CONT(interpreterMain) {
    PROLOGUE(interpreterMain);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;
    const char* name = rt->argv[1];
    --rt->argc;
    memmove(rt->argv + 1, rt->argv + 2, sizeof (char*) * rt->argc);
    Chili mod = cbyModuleLoadByNameOrFile(proc, name);
    if (!chiEither(&mod)) {
        ASET(0, mod);
        KNOWN_JUMP(chiThrowRuntimeException);
    }
    main_z_Main = chiToInt32(chiToCbyModule(mod)->main);
    ASET(0, mod);
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

CHI_EXTERN_CONT_DECL(z_Main)
CONT(z_Main) {
    PROLOGUE(z_Main);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (CHI_AND(CBY_DISASM_ENABLED, cbyInterpreter(proc)->option.disasm))
        disasm(proc);

    KNOWN_JUMP(interpreterMain);
}

CHI_EXTERN_CONT_DECL(z_System__EntryPoints)
CONT(z_System__EntryPoints) {
    PROLOGUE(z_System__EntryPoints);
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiStringRef name = CHI_STRINGREF("System/EntryPoints");
    Chili loaded = findModule(proc->rt, name);
    if (chiTrue(loaded)) {
        ASET(0, loaded);
        RET;
    }

    Chili mod = cbyModuleLoadByName(proc, name);
    if (!chiEither(&mod)) {
        ASET(0, proc->rt->fail);
        RET;
    }

    ASET(0, mod);
    KNOWN_JUMP(cbyModuleInit);
}

CONT(cbyFFIError, .na = 1) {
    PROLOGUE(cbyFFIError);
    const CbyCode* IP = chiToIP(AGET(0));
    UNDEF_ARGS(0);
    ChiStringRef name = FETCH_STRING;
    ASET(0, chiFmtString(CHI_CURRENT_PROCESSOR, "FFI function '%S' not loaded", name));
    KNOWN_JUMP(chiThrowRuntimeException);
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
