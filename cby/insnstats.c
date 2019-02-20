#include "insnstats.h"
#include "asm.h"
#include "mem.h"

#define CYCLES_ENABLED (cycles && CBY_BACKEND_CYCLES_ENABLED)

typedef struct {
    uint64_t count, cycles, cyclesCorr;
    uint16_t op;
} Insn;

#define S_ELEM       Insn
#define S_SUFFIX     InsnByCycles
#define S_LESS(a, b) (b->cyclesCorr < a->cyclesCorr)
#include "sort.h"

#define S_ELEM       Insn
#define S_SUFFIX     InsnByCount
#define S_LESS(a, b) (b->count < a->count)
#include "sort.h"

void cbyPrintInsnTable(CbyInterpreter* interp, const CbyCountStats* count, const CbyCyclesStats* cycles) {
    const ChiTime delta = chiTimeDelta(interp->rt.timeRef.end, interp->rt.timeRef.start);
    uint32_t overhead = chiCpuCyclesOverhead();
    uint64_t totalCount = 0, totalCycles = 0;
    CHI_AUTO_ZALLOC(Insn, insn, OPCODE_COUNT);
    for (uint16_t i = 0; i < OPCODE_COUNT; ++i) {
        insn[i].op = i;
        insn[i].count = count->insn[i];
        totalCount += insn[i].count;
        if (CYCLES_ENABLED) {
            insn[i].cycles = cycles->insn[i];
            insn[i].cyclesCorr = (uint64_t)CHI_MAX((double)insn[i].cycles - (double)insn[i].count * overhead, 0);
            totalCycles += insn[i].cycles;
        }
    }

    ChiStats* stats = &interp->rt.stats;
    chiStatsTitle(stats, "interpreter");

    chiStatsRow(stats, "calls");
    chiStatsIntUnit(stats, "enter", count->enter, true, 0);
    chiStatsIntUnit(stats, "jmp", count->enterJmp, true, 0);
    chiStatsPercent(stats, "ratio", count->enterJmp ? (double)count->enter / (double)count->enterJmp : 0);

    chiStatsRow(stats, "ffi calls");
    chiStatsIntUnit(stats, "inline", count->insn[OP_ffiinl], true, 0);
    chiStatsIntUnit(stats, "tail", count->insn[OP_ffitail], true, 0);

    chiStatsRow(stats, "insn");
    chiStatsIntUnit(stats, "executed", totalCount, true, 0);
    chiStatsFloatUnit(stats, "rate", chiPerSec(totalCount, delta.cpu), true, "/s");

    if (cycles)
        sortInsnByCycles(insn, OPCODE_COUNT);
    else
        sortInsnByCount(insn, OPCODE_COUNT);

    const ChiRuntimeOption* opt = &interp->rt.option;
    uint32_t rows = CHI_MIN(opt->stats->tableRows, OPCODE_COUNT);
    uint64_t* countCol = chiAllocArr(uint64_t, rows);
    ChiStringRef* nameCol = chiAllocArr(ChiStringRef, rows);
    uint64_t* cyclesCol = 0, *cyclesCorrCol = 0;
    double* cyclesPerInsnCol = 0;
    if (CYCLES_ENABLED) {
        cyclesCol = chiAllocArr(uint64_t, rows);
        cyclesCorrCol = chiAllocArr(uint64_t, rows);
        cyclesPerInsnCol = chiAllocArr(double, rows);
    }

    for (uint32_t i = 0; i < rows; ++i) {
        nameCol[i] = chiStringRef(cbyOpName[insn[i].op]);
        countCol[i] = insn[i].count;
        if (CYCLES_ENABLED) {
            cyclesCol[i] = insn[i].cycles;
            cyclesCorrCol[i] = insn[i].cyclesCorr;
            cyclesPerInsnCol[i] = insn[i].count ? (double)insn[i].cyclesCorr / (double)insn[i].count : 0;
        }
    }

    ChiStatsColumn column[8], *col = column;
    if (CYCLES_ENABLED) {
        uint64_t cyclesDelta = chiCpuCycles() - cycles->begin;
        double interpTime = (double)cycles->interp * (double)CHI_UN(Nanos, delta.real) / (double)cyclesDelta;

        chiStatsRow(stats, "interp");
        chiStatsTime(stats, "time", (ChiNanos){(uint64_t)interpTime});
        chiStatsPercent(stats, "ratio", interpTime / (double)CHI_UN(Nanos, delta.cpu));

        chiStatsRow(stats, "cycles");
        chiStatsFloat(stats, "cy/insn", totalCount ? CHI_MAX((double)totalCycles / (double)totalCount - overhead, 0) : 0);
        chiStatsInt(stats, "overhead/insn", overhead);

        *col++ = (ChiStatsColumn){ .header = "%",      .type = CHI_STATS_INT, .ints = cyclesCorrCol, .sum = totalCycles - totalCount * overhead, .percent = true };
        *col++ = (ChiStatsColumn){ .header = "cycles", .type = CHI_STATS_INT, .ints = cyclesCorrCol, .sep = true };
        *col++ = (ChiStatsColumn){ .header = "%",      .type = CHI_STATS_INT, .ints = cyclesCol,     .sum = totalCycles, .percent = true };
        *col++ = (ChiStatsColumn){ .header = "cy+ovh", .type = CHI_STATS_INT, .ints = cyclesCol,     .sep = true };
    }

    *col++ = (ChiStatsColumn){ .header = "%",     .type = CHI_STATS_INT, .ints = countCol, .sum = totalCount, .percent = true };
    *col++ = (ChiStatsColumn){ .header = "count", .type = CHI_STATS_INT, .ints = countCol, .sep = true };
    if (CYCLES_ENABLED)
        *col++ = (ChiStatsColumn){ .header = "cy/insn", .type = CHI_STATS_FLOAT,  .floats  = cyclesPerInsnCol, .sep = true };
    *col++ = (ChiStatsColumn){ .header = "instruction", .type = CHI_STATS_STRING, .strings = nameCol, .left = true };

    CHI_ASSERT((uint32_t)(col - column) <= CHI_DIM(column));
    chiStatsTable(stats, .title = "instruction", .rows = rows, .columns = (uint32_t)(col - column), .data = column);
}
