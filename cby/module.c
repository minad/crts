#include <chili/cont.h>
#include <stdio.h>
#include "adt.h"
#include "cby.h"
#include "event.h"
#include "exception.h"
#include "stack.h"
#include "export.h"
#include "runtime.h"
#include "sink.h"
#include "mem.h"
#include "bytecode/decode.h"

struct ChiModuleEntry_ {
    Chili pair;
};

#define NOHASH
#define HASH        ChiModuleHash
#define ENTRY       ChiModuleEntry
#define PREFIX      moduleHash
#define KEY(e)      chiStringRef(&chiToCbyModule(CHI_IDX(e->pair, 1))->name)
#define EXISTS(e)   chiTrue(e->pair)
#define KEYEQ(a, b) chiStringRefEq(a, b)
#define HASHFN      chiHashStringRef
#include "hashtable.h"

int32_t
    z_System_2fEntryPoints_z_exit,
    z_System_2fEntryPoints_z_par,
    z_System_2fEntryPoints_z_scheduler,
    z_System_2fEntryPoints_z_unhandled,
    z_System_2fEntryPoints_z_interrupt,
    z_Main_z_main;

static Chili findModule(ChiRuntime* rt, ChiStringRef name) {
    ChiModuleEntry* mod = moduleHashFind(&rt->moduleHash, name);
    return mod ? CHI_IDX(mod->pair, 0) : CHI_FALSE;
}

CHI_CONT_DECL(static, loadModule)
CHI_CONT_DECL(static, interpInitImport)
CHI_CONT_DECL(static, replNext)

STATIC_CONT(interpInitModuleCont,, .size = 2) {
    PROLOGUE(interpInitModuleCont);
    SP -= 2;
    Chili mod = SP[0];
    Chili value = A(0);
    ChiStringRef name = chiStringRef(&chiToCbyModule(mod)->name);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    moduleHashInsert(&proc->rt->moduleHash, moduleHashIndex(name))->pair =
        chiRoot(proc->rt, chiNewPinTuple(value, mod));
    CHI_EVENT(proc, MODULE_INIT, .module = name);
    RET(value);
}

STATIC_CONT(interpInitImportCont,, .size = 3) {
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

STATIC_CONT(interpInitImport,, .na = 1) {
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

STATIC_CONT(initModule) {
    PROLOGUE(initModule);

    Chili mod = A(0);
    CbyModule* m = chiToCbyModule(mod);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    Chili loaded = findModule(proc->rt, chiStringRef(&m->name));
    if (chiTrue(loaded))
        RET(loaded);

    uint32_t nimports = chiTrue(m->imports) ? (uint32_t)chiSize(m->imports) : 0;
    LIMITS(.stack = 2 + nimports + CBY_CONTEXT_SIZE, .heap = CHI_SIZEOF_WORDS(CbyFn));

    SP[0] = mod;
    SP[1] = chiFromCont(&interpInitModuleCont);
    SP += 2;

    for (uint32_t i = 0; i < nimports; ++i)
        SP[i] = chiArrayRead(m->imports, i);
    SP += nimports;

    if (chiType(mod) == CBY_NATIVE_MODULE) {
        *SP++ = chiToCbyNativeModule(mod)->init;
        CHI_ASSERT(chiFrameSize(SP - 1) == nimports + 1);
    } else {
        CbyInterpModule* im = chiToCbyInterpModule(mod);
        const CbyCode* IP = chiToIP(im->init);
        uint32_t initOffset = FETCH32;
        Chili init = chiFromIP(IP + initOffset);
        CbyInterpreter* interp = cbyInterpreter(proc);
        if (interp->backend.impl->rewrite) {
            uint32_t codeSize = FETCH32;
            interp->backend.impl->rewrite(CHI_CONST_CAST(IP, CbyCode*), IP + codeSize);
        }

        Chili fn = NEW(CHI_FIRST_FN, CHI_SIZEOF_WORDS(CbyFn));
        CbyFn* f = chiToCbyFn(fn);
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
    CbyInterpreter* interp = cbyInterpreter(proc);
    ChiStringRef name = chiStringRef(&A(0));
    Chili loaded = findModule(proc->rt, name);
    if (chiTrue(loaded))
        RET(loaded);

    Chili mod = cbyLoadByName(&interp->ml, name);
    if (!chiEither(&mod))
        CHI_THROW_RUNTIME_EX(proc, mod);

    A(0) = mod;
    KNOWN_JUMP(initModule);
}

STATIC_CONT(replRead) {
    PROLOGUE(replRead);
    uint32_t cmd;
    PROTECT_BEGIN;
    if (fread(&cmd, sizeof (cmd), 1, stdin) != 1)
        cmd = ~0U;
    PROTECT_END(chiFromUInt32(cmd));
}

STATIC_CONT(replLoadCont,, .size = 1) {
    PROLOGUE(replLoadCont);
    --SP;
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;
    uint32_t cmd = chiToUInt32(A(0)), size = cmd >> 8;
    switch (cmd & 0xFF) {
    case 0:
        {
            CbyInterpreter* interp = cbyInterpreter(proc);
            Chili mod = cbyLoadFromStdin(&interp->ml, size);
            if (!chiEither(&mod))
                CHI_THROW_RUNTIME_EX(proc, mod);
            A(0) = mod;
            KNOWN_JUMP(initModule);
        }

    case 1:
        {
            uint8_t bytes[256];
            if (size > sizeof (bytes) || fread(bytes, size, 1, stdin) != 1) {
                A(0) = CHI_TRUE;
                KNOWN_JUMP(chiExitDefault);
            }
            ChiStringRef name = { .bytes = bytes, .size = size };
            ChiModuleEntry* entry = moduleHashFind(&rt->moduleHash, name);
            if (entry) {
                chiUnroot(rt, entry->pair);
                entry->pair = CHI_FALSE;
                --rt->moduleHash.used;
                CHI_EVENT(proc, MODULE_UNLOAD, .module = name);
            }
            RET(CHI_FALSE);
        }

    default:
        A(0) = CHI_FALSE;
        KNOWN_JUMP(chiExitDefault);
    }
}

STATIC_CONT(replMain) {
    PROLOGUE(replMain);
    LIMITS(.stack = 2);

    SP[0] = chiFromCont(&replNext);
    SP[1] = chiFromCont(&replLoadCont);
    SP += 2;

    KNOWN_JUMP(replRead);
}

STATIC_CONT(replNext,, .size = 1) {
    PROLOGUE(replNext);
    --SP;
    chiSinkPutc(chiStdout, '\n');
    chiSinkFlush(chiStdout);
    KNOWN_JUMP(replMain);
}

STATIC_CONT(interpreterMain) {
    PROLOGUE(interpreterMain);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CbyInterpreter* interp = cbyInterpreter(proc);

    const char* name = proc->rt->argv[1];
    --proc->rt->argc;
    memmove(proc->rt->argv + 1, proc->rt->argv + 2, sizeof (char*) * proc->rt->argc);
    Chili mod = cbyLoadByNameOrFile(&interp->ml, name);
    if (!chiEither(&mod))
        CHI_THROW_RUNTIME_EX(proc, mod);

    z_Main_z_main = cbyModuleExport(mod, "main");

    A(0) = mod;
    KNOWN_JUMP(initModule);
}

CONT(chiExit) {
    PROLOGUE(chiExit);
    CbyInterpreter* interp = cbyInterpreter(CHI_CURRENT_PROCESSOR);
    if (interp->option.repl) {
        chiSinkPutc(chiStdout, '\n');
        chiSinkFlush(chiStdout);
        KNOWN_JUMP(replMain);
    }
    KNOWN_JUMP(chiExitDefault);
}

CONT(z_Main) {
    PROLOGUE(z_Main);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CbyInterpreter* interp = cbyInterpreter(proc);
    if (CHI_AND(CBY_DISASM_ENABLED, interp->option.disasm)) {
        CHI_AUTO_SINK(sink, chiStdoutColor(interp->rt.option.color));
        for (uint32_t i = 1; i < interp->rt.argc; ++i) {
            Chili mod = cbyLoadByNameOrFile(&interp->ml, interp->rt.argv[i]);
            if (!chiEither(&mod))
                CHI_THROW_RUNTIME_EX(proc, mod);
            if (chiType(mod) != CBY_INTERP_MODULE)
                CHI_THROW_RUNTIME_EX(CHI_CURRENT_PROCESSOR,
                                     chiFmtString("Cannot disassemble native module '%s'",
                                                  interp->rt.argv[i]));
            Chili code = chiToCbyInterpModule(mod)->code;
            CHI_IGNORE_RESULT(cbyDisasm(sink, cbyCode(code), cbyCodeEnd(code)));
        }
        A(0) = CHI_FALSE;
        KNOWN_JUMP(chiExit);
    }

    if (interp->option.repl)
        KNOWN_JUMP(replMain);

    KNOWN_JUMP(interpreterMain);
}

#define LOAD_FUNCTION(mod, name, fn)                                    \
    ({                                                                  \
        int32_t idx = cbyModuleExport((mod), (fn));                    \
        if (idx < 0)                                                    \
            CHI_THROW_RUNTIME_EX(proc,                                  \
                                 chiFmtString("Could not load exported function '%s' from '%S'.", \
                                               (fn), (name)));          \
        idx;                                                            \
    })

CONT(z_System_2fEntryPoints) {
    PROLOGUE(z_System_2fEntryPoints);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CbyInterpreter* interp = cbyInterpreter(proc);
    ChiStringRef name = CHI_STRINGREF("System/EntryPoints");
    Chili loaded = findModule(proc->rt, name);
    if (chiTrue(loaded))
        RET(loaded);

    Chili mod = cbyLoadByName(&interp->ml, name);
    if (!chiEither(&mod))
        RET(CHI_FAIL);

    z_System_2fEntryPoints_z_exit      = LOAD_FUNCTION(mod, name, "exit");
    z_System_2fEntryPoints_z_par       = LOAD_FUNCTION(mod, name, "par");
    z_System_2fEntryPoints_z_scheduler = LOAD_FUNCTION(mod, name, "scheduler");
    z_System_2fEntryPoints_z_unhandled = LOAD_FUNCTION(mod, name, "unhandled");
    z_System_2fEntryPoints_z_interrupt = LOAD_FUNCTION(mod, name, "interrupt");

    A(0) = mod;
    KNOWN_JUMP(initModule);
}
