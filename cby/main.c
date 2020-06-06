#include <stdlib.h>
#include "cby.h"
#include "native/error.h"
#include "native/sink.h"
#include "native/strutil.h"

static ChiOptionResult addPath(ChiOptionParser* CHI_UNUSED(parser),
                               ChiOptionAssoc* assoc,
                               ChiOption* CHI_UNUSED(desc),
                               const void* val) {
    cbyAddPath(&((CbyInterpreter*)assoc->target)->ml, chiStringRef((const char*)val));
    return CHI_OPTRESULT_OK;
}

#define FOREACH_BACKEND(BACKEND, SEP)                             \
    BACKEND(, default)                                            \
    CHI_IF(CBY_BACKEND_COUNT_ENABLED,  BACKEND(SEP, count  ))     \
    CHI_IF(CBY_BACKEND_CYCLES_ENABLED, BACKEND(SEP, cycles ))     \
    CHI_IF(CBY_BACKEND_CALLS_ENABLED,  BACKEND(SEP, calls  ))     \
    CHI_IF(CBY_BACKEND_DUMP_ENABLED,   BACKEND(SEP, dump   ))     \
    CHI_IF(CBY_BACKEND_PROF_ENABLED,   BACKEND(SEP, prof   ))     \
    CHI_IF(CBY_BACKEND_FNLOG_ENABLED,  BACKEND(SEP, fnlog  ))

#define _BACKEND(s, n) extern const CbyBackend n##_interpBackend;
FOREACH_BACKEND(_BACKEND,)
#undef _BACKEND

#define _BACKEND(s, n) &n##_interpBackend,
static const CbyBackend* backends[] = { FOREACH_BACKEND(_BACKEND,) };
#undef _BACKEND

#define CHI_OPT_STRUCT CbyInterpreter
#define _EVFMT(N, n) #n
#define _BACKEND(s, n) s #n
CHI_IF(CHI_EVENT_CONVERT_ENABLED,
       static CHI_OPT_GROUP(evconvOptions, "EVENT CONVERTER",
                            CHI_OPT_CHOICE(option.evconv, "evconv", "Output format", CHI_FOREACH_EVENT_FORMAT(_EVFMT, ", "))))
static CHI_OPT_GROUP(interpOptions, "INTERPRETER",
                     CHI_OPT_CB(STRING, addPath, "path", "Search path")
                     CHI_IF(CBY_LSMOD_ENABLED, CHI_OPT_FLAG(option.lsmod, "lsmod", "List modules on path"))
                     CHI_IF(CBY_DISASM_ENABLED, CHI_OPT_FLAG(option.disasm, "asm", "Print module bytecode assembly"))
                     CHI_OPT_CHOICE(backend.id, "backend", "Interpreter backend", FOREACH_BACKEND(_BACKEND, ", "))
                     CHI_IF(CBY_BACKEND_DUMP_ENABLED, CHI_OPT_FLAG(option.dumpFile, "dump:f", "Dump instructions to file"))
                     CHI_IF(CBY_BACKEND_DUMP_ENABLED, CHI_OPT_SIZE(option.dumpBuffer, 0, CHI_MiB(64), "dump:b", "Dump buffer size")))
#undef _BACKEND
#undef _EVFMT
#undef CHI_OPT_STRUCT

static void interpDestroy(ChiHookType CHI_UNUSED(type), ChiProcessor* proc) {
    if (chiProcessorMain(proc))
        cbyModuleLoaderDestroy(&cbyInterpreter(proc)->ml);
}

static ChiOptionResult parseInterpOptions(CbyInterpreter* interp, ChiChunkOption* chunkOption,
                                          int* argc, char** argv) {
    CHI_AUTO_SINK(helpSink, chiSinkColor(CHI_SINK_COLOR_AUTO));
    ChiRuntimeOption* opt = &interp->rt.option;
    ChiOptionParser parser =
        { .help = helpSink,
          .usage = chiStderr,
          .assocs = (ChiOptionAssoc[])
          { { &chiGeneralOptions, opt },
            { &chiStackOptions, opt },
            { &chiNurseryOptions, opt },
            { &chiHeapOptions, opt },
            { &chiGCOptions, opt },
            { &chiChunkOptions, chunkOption },
            CHI_IF(CHI_EVENT_ENABLED, { &chiEventOptions, opt },)
            CHI_IF(CHI_EVENT_CONVERT_ENABLED, { &evconvOptions, interp },)
            CHI_IF(CHI_STATS_ENABLED, { &chiStatsOptions, opt },)
            { &interpOptions, interp },
            CHI_IF(CBY_BACKEND_PROF_ENABLED, { &chiProfOptions, &interp->option.prof },)
            { 0 }
          }
        };
    ChiOptionResult res = chiOptionEnv(&parser, "CHI_NATIVE_OPTS");
    if (res == CHI_OPTRESULT_OK)
        res = chiOptionEnv(&parser, "CBY_OPTS");
    if (res == CHI_OPTRESULT_OK)
        res = chiOptionArgs(&parser, argc, argv);
    if (res == CHI_OPTRESULT_HELP) {
        chiPager();
        chiOptionHelp(&parser);
    }
    return res;
}

// We allow to use more stack space here to store
// the rather large CbyInterpreter struct.
CHI_WARN_OFF(frame-larger-than=)
int main(int argc, char** argv) {
    chiSystemSetup();

    ChiChunkOption chunkOption = CHI_DEFAULT_CHUNK_OPTION;
    CbyInterpreter interp =
        { .option = { CHI_IF(CBY_BACKEND_PROF_ENABLED, .prof = CHI_DEFAULT_PROF_OPTION,)
                      CHI_IF(CBY_BACKEND_DUMP_ENABLED, .dumpBuffer = CHI_MiB(4)) } };
    ChiRuntime* rt = &interp.rt;
    chiRuntimeInit(rt, exit);
    chiHookProc(rt, STOP, interpDestroy);

    ChiOptionResult res = parseInterpOptions(&interp, &chunkOption, &argc, argv);
    if (res != CHI_OPTRESULT_OK)
        return res == CHI_OPTRESULT_ERROR ? CHI_EXIT_ERROR : CHI_EXIT_SUCCESS;

    for (int i = 1; i < argc; ++i) {
        const char* s = argv[i];
        if (cbyLibrary(s))
            cbyAddPath(&interp.ml, (ChiStringRef){ .size = (uint32_t)(strchrnul(s, ':') - s),
                                                   .bytes = (const uint8_t*)s });
    }

    CHI_AUTO_FREE(path, chiGetEnv("CBY_PATH"));
    if (path)
        cbyAddPath(&interp.ml, chiStringRef(path));

    if (CHI_AND(CBY_LSMOD_ENABLED, interp.option.lsmod)) {
        CHI_AUTO_SINK(sink, chiSinkColor(CHI_SINK_TEXT));
        chiPager();
        cbyModuleListing(&interp.ml, sink);
        return CHI_EXIT_SUCCESS;
    }

#if CHI_EVENT_CONVERT_ENABLED
    ChiEventFormat fmt = interp.option.evconv;
    if (fmt) {
        ChiFile file = CHI_FILE_STDIN;
        const char* name = "stdin";
        if (argc >= 2) {
            name = argv[1];
            file = chiFileOpenRead(name);
            if (chiFileNull(file))
                chiErr("Could not open '%s': %m", name);
        }
        const ChiRuntimeOption* opt = &rt->option;
        CHI_AUTO_SINK(sink, chiSinkFileTryNew(argc < 3 ? "stdout" : argv[2],
                                              0, false,
                                              CHI_AND(CHI_COLOR_ENABLED,
                                                      fmt == CHI_EVFMT_PRETTY ? opt->color
                                                      : fmt == CHI_EVFMT_BIN ? CHI_SINK_BINARY
                                                      : CHI_SINK_TEXT)));
        if (!sink)
            return CHI_EXIT_ERROR;
        if (argc < 3 && fmt == CHI_EVFMT_PRETTY)
            chiPager();
        chiEventConvert(name, file, sink, fmt, opt->event.bufSize * opt->event.msgSize);
        if (argc >= 2)
            chiFileClose(file);
        return CHI_EXIT_SUCCESS;
    }
#endif

    if (argc < 2) {
        chiSinkFmt(chiStderr, "Usage: %s [-Rhelp] [-R...] [module|file] args...\n", argv[0]);
        return CHI_EXIT_ERROR;
    }

    interp.backend.impl = backends[interp.backend.id];
#if CBY_BACKEND_PROF_ENABLED
    if (interp.option.prof.file[0] || interp.option.prof.flat || interp.option.prof.alloc)
        interp.backend.impl = &prof_interpBackend;
    else if (interp.backend.impl == &prof_interpBackend)
        interp.option.prof.flat = true;
#endif

    chiHookProc(rt, START, interp.backend.impl->procStart);
    chiHookProc(rt, STOP, interp.backend.impl->procStop);
    interp.backend.impl->setup(&interp);
    chiChunkSetup(&chunkOption);
    chiRuntimeEnter(rt, argc, argv);
}
CHI_WARN_ON
