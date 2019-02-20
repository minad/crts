#include "cby.h"
#include "sink.h"
#include "mem.h"
#include "chunk.h"
#include <stdlib.h>

static ChiOptionResult addPath(const ChiOptionParser* CHI_UNUSED(parser),
                               const ChiOptionList* list,
                               const ChiOption* CHI_UNUSED(desc),
                               const void* val) {
    cbyAddPath(&((CbyInterpreter*)list->target)->ml, chiStringRef((const char*)val));
    return CHI_OPTRESULT_OK;
}

#define FOREACH_BACKEND(SEP, BACKEND)                             \
    BACKEND(, default)                                            \
    CHI_IF(CBY_BACKEND_COUNT_ENABLED,  BACKEND(SEP, count  ))     \
    CHI_IF(CBY_BACKEND_CYCLES_ENABLED, BACKEND(SEP, cycles ))     \
    CHI_IF(CBY_BACKEND_CALLS_ENABLED,  BACKEND(SEP, calls  ))     \
    CHI_IF(CBY_BACKEND_DUMP_ENABLED,   BACKEND(SEP, dump   ))     \
    CHI_IF(CBY_BACKEND_PROF_ENABLED,   BACKEND(SEP, prof   ))     \
    CHI_IF(CBY_BACKEND_TRACE_ENABLED,  BACKEND(SEP, trace  ))

#define _BACKEND(s, n) extern const CbyBackend n##_interpBackend;
FOREACH_BACKEND(, _BACKEND)
#undef _BACKEND

#define _BACKEND(s, n) &n##_interpBackend,
static const CbyBackend* backends[] = { FOREACH_BACKEND(, _BACKEND) };
#undef _BACKEND

#define CHI_OPT_TARGET CbyInterpreter
static const ChiOption interpOptionList[] = {
    CHI_OPT_DESC_TITLE("INTERPRETER")
    CHI_OPT_DESC_CB(STRING, addPath, "path", "Search path")
    CHI_IF(CBY_LSMOD_ENABLED, CHI_OPT_DESC_FLAG(option.lsmod, "lsmod", "List modules on path"))
    CHI_OPT_DESC_FLAG(option.repl, "repl", "Repl module loader")
    CHI_IF(CBY_DISASM_ENABLED, CHI_OPT_DESC_FLAG(option.disasm, "asm", "Print module bytecode assembly"))
#define _BACKEND(s, n) s #n
    CHI_OPT_DESC_CHOICE(backend.id, "backend", "Interpreter backend", FOREACH_BACKEND(", ", _BACKEND))
#undef _BACKEND
    CHI_IF(CBY_BACKEND_DUMP_ENABLED, CHI_OPT_DESC_FLAG(option.dumpFile, "dfile", "Dump trace to file"))
    CHI_OPT_DESC_END
};
#undef CHI_OPT_TARGET

static const CbyOption defaults = { CHI_IF(CBY_BACKEND_PROF_ENABLED, .prof[0] = CHI_DEFAULT_PROF_OPTION) };

CHI_DEFINE_AUTO(CbyModuleLoader*, cbyModuleLoaderDestroy)

// We allow to use more stack space here to store
// the rather large CbyInterpreter struct.
CHI_WARN_OFF(frame-larger-than=)
int main(int argc, char** argv) {
    chiSystemSetup();

    ChiChunkOption chunkOption = CHI_DEFAULT_CHUNK_OPTION;
    CbyInterpreter interp = { .option = defaults };
    ChiRuntime* rt = &interp.rt;
    chiInit(rt);

    CHI_AUTO(ml, &interp.ml, cbyModuleLoaderDestroy);

    {
        CHI_AUTO_SINK(coloredStdout, chiStdoutColor(CHI_SINK_COLOR_AUTO));
        const ChiOptionList list[] =
            { { chiRuntimeOptionList, &interp.rt.option },
              { chiChunkOptionList, &chunkOption },
              { interpOptionList, &interp },
              CHI_IF(CBY_BACKEND_PROF_ENABLED, { chiProfOptionList, interp.option.prof },)
              { 0, 0 }
            };
        const ChiOptionParser parser =
            { .out = coloredStdout,
              .err = chiStderr,
              .list = list,
            };
        ChiOptionResult res = chiOptionEnv(&parser, "CHI_NATIVE_OPTS");
        if (res != CHI_OPTRESULT_OK)
            return res == CHI_OPTRESULT_ERROR ? CHI_EXIT_ERROR : CHI_EXIT_SUCCESS;
        res = chiOptionEnv(&parser, "CBY_OPTS");
        if (res != CHI_OPTRESULT_OK)
            return res == CHI_OPTRESULT_ERROR ? CHI_EXIT_ERROR : CHI_EXIT_SUCCESS;
        res = chiOptionArgs(&parser, &argc, argv);
        if (res == CHI_OPTRESULT_HELP) {
            chiPager();
            chiOptionHelp(&parser);
            return CHI_EXIT_SUCCESS;
        }
        if (res != CHI_OPTRESULT_OK)
            return res == CHI_OPTRESULT_ERROR ? CHI_EXIT_ERROR : CHI_EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; ++i) {
        const char* s = argv[i];
        if (cbyLibrary(s)) {
            const char* p = strchr(s, ':');
            cbyAddPath(ml, (ChiStringRef){ .size = p ? (uint32_t)(p - s) : (uint32_t)strlen(s),
                                               .bytes = (const uint8_t*)s });
        }
    }

    CHI_AUTO_FREE(path, chiGetEnv("CBY_PATH"));
    if (path)
        cbyAddPath(ml, chiStringRef(path));

    if (CHI_AND(CBY_LSMOD_ENABLED, interp.option.lsmod)) {
        chiPager();
        cbyListModules(ml, chiStdout);
        return CHI_EXIT_SUCCESS;
    }
    if (CHI_AND(CBY_DISASM_ENABLED, interp.option.disasm))
        chiPager();

    if (interp.option.repl ? (argc > 1) : (argc < 2)) {
        chiSinkFmt(chiStderr, "Usage: %s [-Rhelp] [-R...] [module|file] args...\n", argv[0]);
        return CHI_EXIT_ERROR;
    }

    interp.backend.impl = backends[interp.backend.id];
#if CBY_BACKEND_PROF_ENABLED
    if (interp.option.prof->file[0] || interp.option.prof->flat)
        interp.backend.impl = &prof_interpBackend;
    else if (interp.backend.impl == &prof_interpBackend)
        interp.option.prof->flat = true;
#endif

    chiHook(rt, CHI_HOOK_PROC_START, interp.backend.impl->procStart);
    chiHook(rt, CHI_HOOK_PROC_STOP, interp.backend.impl->procStop);
    interp.backend.impl->setup(&interp);
    chiChunkSetup(&chunkOption);
    chiRun(rt, argc, argv);
}
CHI_WARN_ON
