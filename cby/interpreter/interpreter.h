#include <chili/cont.h>
#include <chili/prim.h>
#include "../bytecode/decode.h"
#include "../cby.h"
#include "../ffisupport.h"
#include "macros.h"
#include "native/mem.h"
#include "native/stack.h"

// lint: no-sort
#include "mangling.h"

CHI_WARN_OFF(c++-compat)
typedef struct CbyInterpPS_ {
    InstrumentData data;
    FFIData        ffi;
} CbyInterpPS;

struct CbyInterpState_ {
    InstrumentData data;
};
CHI_WARN_ON

CHI_STATIC_CONT_DECL(interpreter)
CHI_STATIC_CONT_DECL(interpCont)
CHI_STATIC_CONT_DECL(interpFn)
CHI_STATIC_CONT_DECL(interpThunk)

// noclone, noinline to prevent issues with label addresses
// See https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
ATTR_CONT(interpreter,
          static,
          CHI_NOINL
#if __has_attribute(__noclone__)
          __attribute__((noclone))
#endif
          , .na = 3) {
    PROLOGUE(interpreter);
    DISPATCH_PROLOGUE;

    Chili CURRFN = AGET(0);
    const CbyCode* IP = chiToIP(AGET(1));
    Chili* REG = (Chili*)chiToPtr(AGET(2));
    UNDEF_ARGS(0);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CbyInterpPS* interpPS = proc->interpPS;
    ENTER(false);

#include "../bytecode/interpreter.h"

 SAVE_CLOS:
    ASET(0, CURRFN);
    if (chiType(CURRFN) != CHI_THUNK) {
        AUX.VAARGS = (uint8_t)(chiFnArity(CURRFN) + 1);
        for (uint32_t i = 0; i + 1 < AUX.VAARGS; ++i)
            ASET(i + 1, SP[i]);
    }
    KNOWN_JUMP(chiYieldClos);
}

STATIC_CONT(interpCont, .interp = true, .size = CHI_VASIZE, .na = CHI_VAARGS) {
    PROLOGUE(interpCont);
    CHI_ASSERT(chiToCont(SP[-1]) == &interpCont);

    Chili CURRFN, *REG;
    const CbyCode* IP;
    FETCH_ENTER;

    // more precisely .stack=nargs - CBY_CONTEXT_SIZE
    LIMITS_SAVE(({
            SP = top + CBY_CONTEXT_SIZE;
            AUX.VAARGS = (uint8_t)nargs;
            KNOWN_JUMP(chiYieldCont);
        }), .stack = CHI_AMAX - CBY_CONTEXT_SIZE);

    for (uint32_t i = 0; i < nargs; ++i)
        top[i] = AGET(i);
    UNDEF_ARGS(0);

    ASET(0, CURRFN);
    ASET(1, chiFromIP(IP));
    ASET(2, chiFromPtr(REG));
    KNOWN_JUMP(interpreter);
}

STATIC_CONT(interpFn, .na = CHI_VAARGS) {
    PROLOGUE(interpFn);

    const Chili CURRFN = AGET(0);
    CHI_ASSERT(chiFn(chiType(CURRFN)));
    const CbyFn* f = chiToCbyFn(CURRFN);
    const uint32_t nargs = chiFnArity(CURRFN);

    // More precisely .stack=nargs, but overestimating constant is better for faster check
    LIMITS_SAVE(({
                AUX.VAARGS = (uint8_t)(nargs + 1);
                KNOWN_JUMP(chiYieldClos);
            }), .stack = CHI_AMAX);

    // Function prologue, put arguments on the stack
    CHI_ASSERT(SP + nargs <= SL);
    CHI_ASSUME(nargs <= CHI_AMAX);
    for (uint32_t i = 0; i < nargs; ++i)
        SP[i] = AGET(i + 1);

    UNDEF_ARGS(0);

    chiStackCheckCanary(SL);

    ASET(0, CURRFN);
    ASET(1, f->ip);
    ASET(2, chiFromPtr(SP));
    KNOWN_JUMP(interpreter);
}

STATIC_CONT(interpThunk, .na = 1) {
    PROLOGUE(interpThunk);
    const Chili CURRFN = AGET(0);
    UNDEF_ARGS(0);
    CHI_ASSERT(chiType(CURRFN) == CHI_THUNK);
    const CbyFn* f = chiToCbyFn(CURRFN);
    ASET(0, CURRFN);
    ASET(1, f->ip);
    ASET(2, chiFromPtr(SP));
    KNOWN_JUMP(interpreter);
}

static void interpProcStart(ChiHookType CHI_UNUSED(type), ChiProcessor* proc) {
    CbyInterpreter* interp = cbyInterpreter(proc);
    proc->interpPS = sizeof (CbyInterpPS) ? chiZallocObj(CbyInterpPS) : (CbyInterpPS*)-1;
    CHI_LOCK_MUTEX(&interp->backend.mutex);
    instrumentProcStart(proc, &proc->interpPS->data, &interp->backend.state->data);
    ffiSetup(&proc->interpPS->ffi);
}

static void interpProcStop(ChiHookType CHI_UNUSED(type), ChiProcessor* proc) {
    CbyInterpreter* interp = cbyInterpreter(proc);

    {
        CHI_LOCK_MUTEX(&interp->backend.mutex);
        instrumentProcStop(proc, &proc->interpPS->data, &interp->backend.state->data);
    }
    ffiDestroy(&proc->interpPS->ffi);
    if (sizeof (CbyInterpPS))
        chiFree(proc->interpPS);

    if (chiProcessorMain(proc)) {
        instrumentDestroy(interp, &interp->backend.state->data);
        if (sizeof (CbyInterpState))
            chiFree(interp->backend.state);
        chiMutexDestroy(&interp->backend.mutex);
    }
}

static void interpSetup(CbyInterpreter* interp) {
    chiMutexInit(&interp->backend.mutex);
    interp->backend.state = sizeof (CbyInterpState) ? chiZallocObj(CbyInterpState) : (CbyInterpState*)-1;
    instrumentSetup(interp, &interp->backend.state->data);
}

extern const CbyBackend interpBackend;
const CbyBackend interpBackend = {
    .cont      = &interpCont,
    .procStop  = &interpProcStop,
    .procStart = &interpProcStart,
    .setup     = &interpSetup,
    .rewrite   = DISPATCH_REWRITE,
};

DISPATCH_INIT
