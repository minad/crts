#include <chili/cont.h>
#include "runtime.h"
#include "stack.h"
#include "event.h"
#include "location.h"
#include "mem.h"

struct ChiModuleEntry_ {
    ChiCont init;
    Chili   val;
};

#define NOHASH
#define HASH       ChiModuleHash
#define ENTRY      ChiModuleEntry
#define PREFIX     moduleHash
#define KEY(e)     e->init
#include "../hashtable.h"

CHI_CONT_DECL(static, initModuleCont)
CHI_CONT_DECL(static, initImportCont)
CHI_CONT_DECL(static, initImport)

STATIC_CONT(initModuleCont,, .size = 2) {
    PROLOGUE(initModuleCont);
    SP -= 2;
    ChiCont init = chiToCont(SP[0]);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiModuleEntry* e = moduleHashInsert(&proc->rt->moduleHash, moduleHashIndex(init));
    e->init = init;
    e->val = chiRoot(proc->rt, A(0));
    if (CHI_EVENT_P(proc, MODULE_INIT)) {
        ChiLocLookup lookup;
        chiLocLookup(&lookup, (ChiLoc){ .native.type = CHI_LOC_NATIVE, .native.cont = init });
        CHI_EVENT(proc, MODULE_INIT, .module = lookup.loc.module);
    }
    RET(A(0));
}

STATIC_CONT(initImportCont,, .size = 3) {
    PROLOGUE(initImportCont);
    SP -= 3;
    Chili ret = A(0);
    uint32_t nimports = chiToUInt32(SP[0]);
    uint32_t idx = chiToUInt32(SP[1]);
    SP[(int32_t)idx - (int32_t)nimports - 2] = ret;
    A(0) = chiFromUInt32(nimports);
    A(1) = chiFromUInt32(idx);
    KNOWN_JUMP(initImport);
}

STATIC_CONT(initImport,, .na = 1) {
    PROLOGUE(initImport);
    LIMITS(.stack = 3);

    uint32_t nimports = chiToUInt32(A(0));
    uint32_t idx = chiToUInt32(A(1));

    if (idx == nimports)
        RET(CHI_FALSE);

    SP[0] = chiFromUInt32(nimports);
    SP[1] = chiFromUInt32(idx + 1);
    SP[2] = chiFromCont(&initImportCont);
    SP += 3;

    JUMP(chiToCont(SP[(int32_t)idx + 1 - (int32_t)nimports - 2 - 3]));
}

CONT(chiInitModule,, .na = 2) {
    PROLOGUE(chiInitModule);

    ChiCont init = chiToCont(A(0));
    uint32_t nimports = chiToUInt32(A(1));
    ChiCont* imports = (ChiCont*)chiToPtr(A(2));
    LIMITS(.stack = 2 + nimports + 1);

    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    ChiModuleEntry* e = moduleHashFind(&rt->moduleHash, init);
    if (e)
        RET(e->val);

    SP[0] = chiFromCont(init);
    SP[1] = chiFromCont(&initModuleCont);
    SP += 2;

    for (uint32_t i = 0; i < nimports; ++i)
        SP[i] = chiFromCont(imports[i]);
    SP[nimports] = chiFromCont(init);
    SP += nimports + 1;
    CHI_ASSERT(chiContInfo(init)->size == nimports + 1);
    CHI_ASSERT(chiFrameSize(SP - 1) == nimports + 1);

    A(0) = chiFromUInt32(nimports);
    A(1) = CHI_FALSE;
    KNOWN_JUMP(initImport);
}
