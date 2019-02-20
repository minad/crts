#pragma once

#include "event.h"
#include "../bytecode/decode.h"
#include "../cby.h"

#define TRACE_EVENT(e, fn)                              \
    ({                                                  \
        if (CHI_EVENT_P(proc, TRACE_##e)) {             \
            ChiEventTrace event;                        \
            cbyReadFnLocation(fn, &event);              \
            CHI_EVENT_STRUCT(proc, TRACE_##e, &event);  \
        }                                               \
    })

typedef int traceData_t[0];
CHI_INL void traceAlloc(ChiProcessor* CHI_UNUSED(proc), traceData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void traceFFIEnd(ChiProcessor* CHI_UNUSED(proc), traceData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void traceInsnBegin(ChiProcessor* CHI_UNUSED(proc), traceData_t* CHI_UNUSED(local), const CbyCode* CHI_UNUSED(ip), Opcode CHI_UNUSED(op)) {}
CHI_INL void traceInsnEnd(ChiProcessor* CHI_UNUSED(proc), traceData_t* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void traceSetup(CbyInterpreter* CHI_UNUSED(interp), traceData_t* CHI_UNUSED(global)) {}
CHI_INL void traceDestroy(CbyInterpreter* CHI_UNUSED(interp), traceData_t* CHI_UNUSED(global)) {}
CHI_INL void traceProcStart(ChiProcessor* CHI_UNUSED(proc), traceData_t* CHI_UNUSED(local), traceData_t* CHI_UNUSED(global)) {}
CHI_INL void traceProcStop(ChiProcessor* CHI_UNUSED(proc), traceData_t* CHI_UNUSED(local), traceData_t* CHI_UNUSED(global)) {}

static CHI_NOINL void traceEnter(ChiProcessor* proc, traceData_t* CHI_UNUSED(local), Chili fn, Chili* CHI_UNUSED(frame), bool jmp) {
    if (jmp)
        TRACE_EVENT(ENTER_JMP, fn);
    else
        TRACE_EVENT(ENTER, fn);
}

static CHI_NOINL void traceLeave(ChiProcessor* proc, traceData_t* CHI_UNUSED(local), Chili fn, Chili* CHI_UNUSED(frame)) {
    TRACE_EVENT(LEAVE, fn);
}

static CHI_NOINL void traceFFIBegin(ChiProcessor* proc, traceData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* ffi) {
    if (CHI_EVENT_P(proc, TRACE_FFI)) {
        const CbyCode* IP = ffi + 4;
        ChiStringRef name = FETCH_STRING;
        CHI_EVENT(proc, TRACE_FFI, .name = name);
    }
}

CHI_INL void traceFFITail(ChiProcessor* proc, traceData_t* local, Chili fn, Chili* frame, const CbyCode* ffi) {
    traceFFIBegin(proc, local, fn, frame, ffi);
}
