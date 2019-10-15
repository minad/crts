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

// noclone, noinline to prevent issues with label addresses
// See https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
ATTR_CONT(interpreter,
          static,
          CHI_NOINL
#if __has_attribute(__noclone__)
          __attribute__((noclone))
#endif
          , .na = 1) {
    PROLOGUE(interpreter);
    DISPATCH_PROLOGUE;
    DECLARE_CONTEXT;

    CURRFN = A(0);
    IP = chiToIP(A(1));
    Chili* REG = SP;

#include "../bytecode/interpreter.h"

 SAVE_FN:
    A(0) = CURRFN;
    for (uint32_t i = 0; i < chiFnOrThunkArity(CURRFN); ++i)
        A(i + 1) = SP[i];
    SP[0] = chiFromCont(&interpFn);
    KNOWN_JUMP(chiInterrupt);

 SAVE_CONT:
    CHI_ASSERT(chiToCont(SP[-1]) == &interpCont);
    A(0) = *SP;
    *SP = chiFromCont(&interpCont);
    KNOWN_JUMP(chiInterrupt);
}

STATIC_CONT(interpCont, .type = CHI_FRAME_INTERP, .size = CHI_VASIZE) {
    PROLOGUE(interpCont);

    DECLARE_CONTEXT;
    RESTORE_CONTEXT(A(0));

    A(0) = CURRFN;
    A(1) = chiFromIP(IP);

    // Use SP-... since we have to skip the top most continuation frame
    instrumentEnter(proc, &interpPS->data, CURRFN, SP - chiToUnboxed(SP[-2]), false);
    KNOWN_JUMP(interpreter);
}

STATIC_CONT(interpFn, .na = CHI_VAARGS) {
    PROLOGUE(interpFn);

    const Chili CURRFN = A(0);
    const CbyFn* f = chiToCbyFn(CURRFN);
    const size_t nargs = chiFnOrThunkArity(CURRFN);
    //LIMITS(.stack=nargs);
    LIMITS(.stack = CHI_AMAX);

    // Function prologue, put arguments on the stack
    CHI_ASSERT(SP + nargs <= SL);
    CHI_ASSUME(nargs <= CHI_AMAX);
    for (size_t i = 0; i < nargs; ++i)
        SP[i] = A(i + 1);
    chiStackCheckCanary(SL);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CbyInterpPS* interpPS = proc->interpPS;
    instrumentEnter(proc, &interpPS->data, CURRFN, SP, false);
    A(1) = f->ip;
    KNOWN_JUMP(interpreter);
}

static void interpProcStart(ChiHookType CHI_UNUSED(type), void* ctx) {
    ChiProcessor* proc = (ChiProcessor*)ctx;
    CbyInterpreter* interp = cbyInterpreter(proc);
    proc->interpPS = sizeof (CbyInterpPS) ? chiZallocObj(CbyInterpPS) : (CbyInterpPS*)-1;
    CHI_LOCK_MUTEX(&interp->backend.mutex);
    instrumentProcStart(proc, &proc->interpPS->data, &interp->backend.state->data);
    ffiSetup(&proc->interpPS->ffi);
}

static void interpProcStop(ChiHookType CHI_UNUSED(type), void* ctx) {
    ChiProcessor* proc = (ChiProcessor*)ctx;
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
