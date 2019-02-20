#include <chili/cont.h>
#include <chili/prim.h>
#include "exception.h"
#include "stack.h"
#include "thread.h"
#include "runtime.h"
#include "ffisupport.h"
#include "sink.h"
#include "asm.h"
#include "mem.h"
#include "bytecode/decode.h"
#include "interpmacros.h"
#include "interpbegin.h"

CHI_WARN_OFF(c++-compat)
typedef struct {
    InstrumentData data;
    FFIData        ffi;
} InterpPS;

typedef struct {
    InstrumentData data;
} InterpState;
CHI_WARN_ON

CHI_INL InterpState* interpGetState(CbyInterpreter* interp) {
    return (InterpState*)interp->backend.state;
}

CHI_INL InterpPS* interpGetPS(ChiProcessor* proc) {
    return (InterpPS*)proc->interpPS;
}

CHI_CONT_DECL(static, interpreter)
CHI_CONT_DECL(static, interpCont)
CHI_CONT_DECL(static, interpFn)

// noclone, noinline to prevent issues with label addresses
// See https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
_CONT(static,
      CHI_NOINL
#if __has_attribute(__noclone__)
      __attribute__((noclone))
#endif
      , interpreter,, .na = 1) {
    PROLOGUE(interpreter);
    DISPATCH_PROLOGUE;
    DECLARE_CONTEXT;

    CURRFN = A(0);
    IP = chiToIP(A(1));
    Chili* REG = SP;

#include "bytecode/interp.h"

 SAVE_FN:
    A(0) = CURRFN;
    for (uint32_t i = 0; i < chiFnOrThunkArity(CURRFN); ++i)
        A(i + 1) = SP[i];
    SP[0] = chiFromCont(&interpFn);
    KNOWN_JUMP(_chiInterrupt);

 SAVE_CONT:
    CHI_ASSERT(chiToCont(SP[-1]) == &interpCont);
    A(0) = *SP;
    *SP = chiFromCont(&interpCont);
    KNOWN_JUMP(_chiInterrupt);
}

STATIC_CONT(interpCont,, .type = CHI_FRAME_INTERP, .size = CHI_VASIZE) {
    PROLOGUE(interpCont);

    DECLARE_CONTEXT;
    RESTORE_CONTEXT(A(0));

    A(0) = CURRFN;
    A(1) = chiFromIP(IP);

    // Use SP-... since we have to skip the top most continuation frame
    instrumentEnter(proc, &interpPS->data, CURRFN, SP - chiToUnboxed(SP[-2]), false);
    KNOWN_JUMP(interpreter);
}

STATIC_CONT(interpFn,, .na = CHI_VAARGS, .type = CHI_FRAME_INTERPFN) {
    PROLOGUE(interpFn);

    const Chili CURRFN = A(0);
    const CbyFn* f = chiToCbyFn(CURRFN);
    const size_t nargs = chiFnOrThunkArity(CURRFN);
    LIMITS(.stack=nargs);

    // Function prologue, put arguments on the stack
    CHI_ASSERT(SP + nargs <= SL);
    CHI_BOUNDED(nargs, CHI_AMAX);
    for (size_t i = 0; i < nargs; ++i)
        SP[i] = A(i + 1);
    chiStackCheckCanary(SL);

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    InterpPS* interpPS = interpGetPS(proc);
    instrumentEnter(proc, &interpPS->data, CURRFN, SP, false);
    A(1) = f->ip;
    KNOWN_JUMP(interpreter);
}

static void interpProcStart(ChiWorker* worker, ChiHookType CHI_UNUSED(type)) {
    ChiProcessor* proc = chiWorkerToProcessor(worker);
    CbyInterpreter* interp = cbyInterpreter(proc);
    InterpPS* interpPS = chiZallocObj(InterpPS);
    proc->interpPS = interpPS;
    CHI_LOCK(&interp->backend.mutex);
    instrumentProcStart(proc, &interpPS->data, &interpGetState(interp)->data);
    ffiSetup(&interpPS->ffi);
}

static void interpProcStop(ChiWorker* worker, ChiHookType CHI_UNUSED(type)) {
    ChiProcessor* proc = chiWorkerToProcessor(worker);
    CbyInterpreter* interp = cbyInterpreter(proc);

    {
        CHI_LOCK(&interp->backend.mutex);
        instrumentProcStop(proc, &interpGetPS(proc)->data, &interpGetState(interp)->data);
    }
    ffiDestroy(&interpGetPS(proc)->ffi);
    chiFree(proc->interpPS);

    if (chiMainWorker(&proc->worker)) {
        instrumentDestroy(interp, &interpGetState(interp)->data);
        chiFree(interp->backend.state);
        chiMutexDestroy(&interp->backend.mutex);
    }
}

static void interpSetup(CbyInterpreter* interp) {
    chiMutexInit(&interp->backend.mutex);
    interp->backend.state = chiZallocObj(InterpState);
    instrumentSetup(interp, &interpGetState(interp)->data);
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

#include "interpend.h"
