#pragma once

#include "../insnstats.h"
#include "sink.h"
#include "color.h"
#include "stack.h"
#include "location.h"

typedef struct {
    ChiSink*       sink;
    CbyFn*         fn;
    const CbyCode* fnBegin, *codeStart;
    uint64_t       count;
} dumpData_t;

CHI_INL void dumpAlloc(ChiProcessor* CHI_UNUSED(proc), dumpData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), size_t CHI_UNUSED(size)) {}
CHI_INL void dumpFFITail(ChiProcessor* CHI_UNUSED(proc), dumpData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void dumpFFIBegin(ChiProcessor* CHI_UNUSED(proc), dumpData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void dumpFFIEnd(ChiProcessor* CHI_UNUSED(proc), dumpData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame), const CbyCode* CHI_UNUSED(ffi)) {}
CHI_INL void dumpInsnEnd(ChiProcessor* CHI_UNUSED(proc), dumpData_t* CHI_UNUSED(local), Opcode CHI_UNUSED(op)) {}
CHI_INL void dumpSetup(CbyInterpreter* CHI_UNUSED(interp), dumpData_t* CHI_UNUSED(global)) {}
CHI_INL void dumpDestroy(CbyInterpreter* CHI_UNUSED(interp), dumpData_t* CHI_UNUSED(global)) {}
CHI_INL void dumpLeave(ChiProcessor* CHI_UNUSED(proc), dumpData_t* CHI_UNUSED(local), Chili CHI_UNUSED(fn), Chili* CHI_UNUSED(frame)) {}

CHI_INL void dumpEnter(ChiProcessor* CHI_UNUSED(proc), dumpData_t* local, Chili fn, Chili* CHI_UNUSED(frame), bool CHI_UNUSED(jmp)) {
    local->fn = chiToCbyFn(fn);
    local->fnBegin = chiToIP(local->fn->ip);
    local->codeStart = cbyInterpModuleCode(local->fn->module);
    ChiLocInfo loc;
    cbyReadLocation(local->codeStart, local->fnBegin, &loc);
    chiSinkFmt(local->sink,
               "         │ "FgWhite"%08x"FgDefault" "TitleBegin"%*L"TitleEnd"%*L\n",
               (uint32_t)(local->fnBegin - local->codeStart),
               CHI_LOCFMT_MODFN, &loc, CHI_LOCFMT_FILE, &loc);
}

CHI_INL void dumpInsnBegin(ChiProcessor* CHI_UNUSED(proc), dumpData_t* local, const CbyCode* ip, Opcode CHI_UNUSED(op)) {
    chiSinkFmt(local->sink, "%8ju │ ", local->count++);
    CHI_IGNORE_RESULT(cbyDisasmInsn(local->sink, ip, local->fnBegin, local->codeStart, (const CbyCode*)-1));
}

CHI_INL void dumpProcStart(ChiProcessor* proc, dumpData_t* local, dumpData_t* CHI_UNUSED(global)) {
    char file[32];
    chiFmt(file, sizeof(file), "insndump.%u.%u.log", chiPid(), proc->worker.wid);
    if (cbyInterpreter(proc)->option.dumpFile)
        local->sink = chiSinkBufferNew(chiSinkFileTryNew(file, proc->rt->option.color), CHI_MiB(4));
    if (!local->sink)
        local->sink = chiStdout;
}

CHI_INL void dumpProcStop(ChiProcessor* CHI_UNUSED(proc), dumpData_t* local, dumpData_t* CHI_UNUSED(global)) {
    chiSinkClose(local->sink);
}
