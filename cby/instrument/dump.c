#include "dump.h"

#if CBY_BACKEND_DUMP_ENABLED

#include "native/color.h"
#include "native/location.h"
#include "native/stack.h"

void _dumpEnter(dumpData* local, Chili fn) {
    local->fn = chiToCbyFn(fn);
    local->fnBegin = chiToIP(local->fn->ip);
    local->codeStart = cbyInterpModuleCode(local->fn->module);
    ChiLocInfo loc;
    cbyReadLocation(local->fnBegin, &loc);
    chiSinkFmt(local->sink,
               "         │ "FgWhite"%08x"FgDefault" "TitleBegin"%*L"TitleEnd"%*L\n",
               (uint32_t)(local->fnBegin - local->codeStart),
               CHI_LOCFMT_FN, &loc, CHI_LOCFMT_FILE, &loc);
}

void _dumpInsnBegin(dumpData* local, const CbyCode* ip) {
    chiSinkFmt(local->sink, "%8ju │ ", local->count++);
    CHI_IGNORE_RESULT(cbyDisasmInsn(local->sink, ip, local->fnBegin, local->codeStart, (const CbyCode*)-1));
}

void _dumpProcStart(ChiProcessor* proc, dumpData* local) {
    char file[64];
    chiFmt(file, sizeof(file), "insndump.%u.%u.log", chiPid(), proc->worker->wid);
    local->sink = chiSinkFileTryNew(cbyInterpreter(proc)->option.dumpFile ? file : "stdout",
                                    CHI_MiB(4), false, proc->rt->option.color);
    if (!local->sink)
        local->sink = chiSinkNull;
}

void _dumpProcStop(dumpData* local) {
    chiSinkClose(local->sink);
}

#endif
