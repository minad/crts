#include <chili/cont.h>
#include "barrier.h"
#include "event.h"
#include "location.h"
#include "stack.h"

struct ChiModuleEntry_ {
    ChiCont init;
    Chili   val;
};

#define HT_NOSTRUCT
#define HT_HASH       ChiModuleHash
#define HT_ENTRY      ChiModuleEntry
#define HT_PREFIX     moduleHash
#define HT_KEY(e)     e->init
#include "generic/hashtable.h"

CHI_STATIC_CONT_DECL(initModuleCont)
CHI_STATIC_CONT_DECL(initImportCont)

STATIC_CONT(initModuleCont, .na = 1, .size = 2) {
    PROLOGUE(initModuleCont);
    Chili res = AGET(0);
    UNDEF_ARGS(0);

    SP -= 2;
    ChiCont init = chiToCont(SP[0]);
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiRuntime* rt = proc->rt;
    ChiModuleEntry* e = moduleHashInsert(&rt->moduleHash, moduleHashFn(init));
    e->init = init;
    e->val = res;

    if (chiEventEnabled(proc, MODULE_INIT)) {
        ChiLocResolve resolve;
        chiLocResolve(&resolve, (ChiLoc){ .type = CHI_LOC_NATIVE, .cont = init },
                     rt->option.stack.traceMangled);
        chiEventStruct(proc, MODULE_INIT, &resolve.loc);
    }

    chiPromoteLocal(proc, &e->val);
    chiGCRoot(rt, e->val);
    ASET(0, e->val);
    RET;
}

STATIC_CONT(initImportCont, .na = 1, .size = CHI_VASIZE) {
    PROLOGUE(initImportCont);
    Chili ret = AGET(0);
    UNDEF_ARGS(0);

    uint32_t nimports = chiToUInt32(SP[-2]) - 4;
    uint32_t idx = chiToUInt32(SP[-3]);
    SP[(int32_t)idx - (int32_t)nimports - 4] = ret;

    if (!idx) {
        SP -= 3;
        RET;
    }

    --idx;
    SP[-3] = chiFromUInt32(idx);
    JUMP_FN(SP[(int32_t)idx - (int32_t)nimports - 4]);
}

CONT(chiInitModule, .na = 3) {
    PROLOGUE(chiInitModule);
    ChiCont init = chiToCont(AGET(0));
    uint32_t nimports = chiToUInt32(AGET(1));
    ChiCont* imports = (ChiCont*)chiToPtr(AGET(2));
    LIMITS_PROC(.stack = 2 + // initModuleCont
                nimports + 1 + // init continuations with all imports
                3); // initImportCont
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiModuleEntry* e = moduleHashFind(&proc->rt->moduleHash, init);
    if (e) {
        ASET(0, e->val);
        RET;
    }

    SP[0] = chiFromCont(init);
    SP[1] = chiFromCont(&initModuleCont);
    SP += 2;

    for (uint32_t i = 0; i < nimports; ++i)
        SP[i] = chiFromCont(imports[i]);
    SP[nimports] = chiFromCont(init);
    SP += nimports + 1;
    CHI_ASSERT(chiContInfo(init)->size == nimports + 1);
    CHI_ASSERT(chiFrameSize(SP - 1) == nimports + 1);

    if (!nimports)
        RET;

    SP[0] = chiFromUInt32(nimports - 1);
    SP[1] = chiFromUInt32(nimports + 4);
    SP[2] = chiFromCont(&initImportCont);
    SP += 3;

    chiStackDebugWalk(chiThreadStack(proc->thread), SP, SL);

    JUMP(chiContFn(imports[nimports - 1]));
}
