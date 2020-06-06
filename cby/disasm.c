#include "bytecode/decode.h"
#include "bytecode/opcodes.h"
#include "cby.h"
#include "ffisupport.h"
#include "native/color.h"
#include "native/location.h"
#include "native/sink.h"

#undef CHECK_RANGE
#define CHECK_RANGE(n) ({ if (IP < codeStart || IP + (n) > codeEnd) goto OUT_OF_RANGE; })

#define REG         FgMagenta"@%u"FgDefault
#define VAL(x)      FgRed x FgDefault
#define NAME(n)     FgGreen #n FgDefault "="
#define FFIREF      FgCyan"<%08x %S>"FgDefault
#define FFIREF_NAME ({ const CbyCode* _oldip = IP; IP = ffiref + 4; const ChiStringRef _name = FETCH_STRING; IP = _oldip; _name; })
#define FFIREF_ARGS (uint32_t)(ffiref - codeStart), FFIREF_NAME
#define FNREF       FgCyan"<%08x %S>"FgDefault
#define FNREF_ARGS  (uint32_t)(fnref - codeStart), ({ cbyReadLocation(fnref, &loc); loc.fn; })

#define _CBY_FFI_TYPE_NAME(name, libffi, dcarg, dccall, type) #type,
static const char* const ffiTypeName[] = { CHI_FOREACH_FFI_TYPE(, _CBY_FFI_TYPE_NAME) };
#undef _CBY_FFI_TYPE_NAME

bool cbyDisasm(ChiSink* sink, const CbyCode* codeStart, const CbyCode* codeEnd) {
    const CbyCode* init = 0, *IP = codeStart + CBY_MAGIC_SIZE;
    uint32_t constSize = FETCH32;
    IP += constSize;

    chiSinkFmt(sink, TitleBegin "module %S"TitleEnd"\n", FETCH_STRING);

    {
        const uint32_t ffiCount = FETCH32;
        const CbyCode* ffiBegin = IP;
        if (ffiCount > 0) {
            chiSinkFmt(sink, "\n"FgWhite"%08x"FgDefault" "TitleBegin"[foreign functions]"TitleEnd"\n", (uint32_t)(ffiBegin - codeStart));
            for (uint32_t i = 0; i < ffiCount; ++i) {
                IP += 4;
                uint32_t off = (uint32_t)(IP - ffiBegin);
                ChiStringRef name = FETCH_STRING;
                uint32_t rtype = FETCH8;
                uint32_t nargs = FETCH8;
                chiSinkFmt(sink, FgWhite"%8u"FgDefault"    #%-*u "NAME(name)VAL("%qS")" "NAME(rtype)VAL("%s")" "NAME(nargs)VAL("%u")" "NAME(atypes)"[",
                           off, OPCODE_MAXLEN - 1, i, name, ffiTypeName[rtype], nargs);
                for (uint32_t j = 0; j < nargs; ++j)
                    chiSinkFmt(sink, "%*w"VAL("%s"), j > 0, ffiTypeName[FETCH8]);
                chiSinkPuts(sink, "]\n");
            }
        }
    }

    {
        const uint32_t importCount = FETCH32;
        const CbyCode* importBegin = IP;
        if (importCount > 0) {
            chiSinkFmt(sink, "\n"FgWhite"%08x"FgDefault" "TitleBegin"[imports]"TitleEnd"\n", (uint32_t)(importBegin - codeStart));
            for (uint32_t i = 0; i < importCount; ++i) {
                ChiStringRef name = FETCH_STRING;
                chiSinkFmt(sink, FgWhite"%8u"FgDefault"    %S\n", i, name);
            }
        }
    }

    {
        FETCH32; // mainIdx
        uint32_t initOff = FETCH32;
        init = IP + initOff;
    }

    {
        uint32_t codeSize = FETCH32;
        if (IP + codeSize + 1 > codeEnd) {
            chiSinkFmt(sink, "Invalid code size %u\n", codeSize);
            return false;
        }
        codeEnd = IP + codeSize;
    }

    while (IP < codeEnd) {
        IP += CBY_FNHEADER;
        ChiLocInfo loc;
        cbyReadLocation(IP, &loc);
        chiSinkFmt(sink, "\n"FgWhite"%08x"FgDefault" " TitleBegin"%s%S"TitleEnd" size=%zu%*L\n",
                   (uint32_t)(IP - codeStart), IP == init ? "[init] " : "",
                   loc.fn, loc.size, CHI_LOCFMT_FILE, &loc);
        for (const CbyCode* fnBegin = IP, *fnEnd = IP + loc.size; IP >= fnBegin && IP < fnEnd;) {
            IP = cbyDisasmInsn(sink, IP, fnBegin, codeStart, codeEnd);
            if (!IP)
                return false;
        }
    }

    chiSinkPutc(sink, '\n');
    return true;

 OUT_OF_RANGE:
    chiSinkFmt(sink, "\nUnexpected read at position %d\n", (int32_t)(IP - codeStart));
    return false;
}

const CbyCode* cbyDisasmInsn(ChiSink* sink, const CbyCode* IP, const CbyCode* fnBegin,
                             const CbyCode* codeStart, const CbyCode* codeEnd) {
    ChiLocInfo loc;
    chiSinkFmt(sink, FgWhite"%8u"FgDefault"    ", (uint32_t)(IP - fnBegin));

    uint16_t opCode = FETCH16;
    if (opCode < OPCODE_PRIM) {
        size_t n = strlen(cbyOpName[opCode]);
        chiSinkFmt(sink, FgBlue"%s"FgDefault"%*w", cbyOpName[opCode], OPCODE_MAXLEN - (int)n);
    } else if (opCode < OPCODE_COUNT) {
        chiSinkFmt(sink, FgBlue"prim"FgDefault"%*w%s", OPCODE_MAXLEN - 3, cbyOpName[opCode]);
    } else {
        chiSinkFmt(sink, "Invalid opcode %d\n", opCode);
        return 0;
    }

    switch (opCode) {
#include "bytecode/disasm.h"
    }

    chiSinkPutc(sink, '\n');
    return IP;

 OUT_OF_RANGE:
    chiSinkFmt(sink, "\nUnexpected read at position %d\n", (int32_t)(IP - codeStart));
    return 0;
}

#undef CHECK_RANGE
#define CHECK_RANGE(n) ({})
