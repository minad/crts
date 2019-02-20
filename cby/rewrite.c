#include "cby.h"
#include "error.h"
#include "ffisupport.h"
#include "sink.h"
#include "bytecode/opcodes.h"
#include "bytecode/decode.h"

typedef struct {
    uint32_t op;
    size_t   start;
    size_t   size;
} DispatchInfo;

#define S_ELEM DispatchInfo
#define S_SUFFIX DispatchInfoByStart
#define S_LESS(a, b) ((a)->start < (b)->start)
#include "sort.h"

#define S_ELEM DispatchInfo
#define S_SUFFIX DispatchInfoBySize
#define S_LESS(a, b) ((a)->size < (b)->size)
#include "sort.h"

static void dumpDispatchInfo(ChiSink* sink, const char* name, uint8_t** start) {
    DispatchInfo info[OPCODE_COUNT];
    for (uint32_t i = 0; i < OPCODE_COUNT; ++i) {
        info[i].op = i;
        info[i].start = (size_t)start[i];
        info[i].size = 0;
    }
    sortDispatchInfoByStart(info, OPCODE_COUNT);

    for (uint32_t i = 0; i < OPCODE_COUNT - 1; ++i)
        info[i].size = (size_t)(info[i + 1].start - info[i].start);

    sortDispatchInfoBySize(info, OPCODE_COUNT);

    chiSinkFmt(sink, "===== Dispatch Info %s =====\n", name);
    for (uint32_t i = 0; i < OPCODE_COUNT - 1; ++i)
        chiSinkFmt(sink, "%-20s %8zu %8zu\n",
                   cbyOpName[info[i].op], info[i].size, info[i].start);
}

void cbyDirectDispatchInit(const char* name, void** start, void** stop, void* startAddress) {
    bool fail = false;
    for (uint32_t i = 0; i < OPCODE_COUNT; ++i) {
        intptr_t opOffset = ((uint8_t*)start[i] - (uint8_t*)startAddress) >> CBY_DIRECT_DISPATCH_SHIFT;
        start[i] = (void*)opOffset;
        if (opOffset > 0xFFFF) {
            chiWarn("%s: Offset for instruction %u is out of range: %zd", name, i, opOffset);
            fail = true;
        }
    }
    if (0)
        dumpDispatchInfo(chiStderr, name, (uint8_t**)start);
    if (fail || stop - start < OPCODE_COUNT)
        chiErr("%s: Invalid instruction offsets found.", name);
}

void cbyDirectDispatchRewrite(void** start, CbyCode* codeStart, const CbyCode* codeEnd) {
    for (CbyCode* IP = codeStart; IP < codeEnd; ) {
        IP += CBY_FNHEADER - 4;
        uint32_t fnSize = FETCH32;
        for (CbyCode* fnEnd = IP + fnSize; IP < fnEnd;) {
            uint16_t opCode = FETCH16;
            chiPokeUInt16(IP - 2, (uint16_t)(uintptr_t)start[opCode]);
            CHI_WARN_OFF(unused-variable)
            switch (opCode) {
#include "bytecode/rewrite.h"
            }
            CHI_WARN_ON
        }
    }
}
