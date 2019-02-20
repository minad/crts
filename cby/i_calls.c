#include <chili/cont.h>

#if CBY_BACKEND_CALLS_ENABLED

#include "sink.h"
#include "mem.h"
#include "location.h"
#include "bytecode/opcodes.h"
#include "instrument/append.h"
#include "instrument/fficall.h"
#include "instrument/fncall.h"

INSTRUMENT_APPEND(fnfficall, fncall, fficall)

static void printFFIStats(ChiStats*, uint32_t, fficallData_t*);
static void printFnStats(ChiStats*, uint32_t, fncallData_t*);

CHI_INL void _fnfficallDestroy(CbyInterpreter* interp, fnfficallData_t* data) {
    const ChiRuntimeOption* opt = &interp->rt.option;
    printFnStats(&interp->rt.stats, opt->stats->tableRows, &data->a);
    printFFIStats(&interp->rt.stats, opt->stats->tableRows, &data->b);
    fnfficallDestroy(interp, data);
}
#define fnfficallDestroy _fnfficallDestroy

#define INSTRUMENT_NAME fnfficall
#define INTERP_NAME     calls
#include "interp.h"

typedef struct {
    uint64_t count, cycles;
    char*    name;
} FnResultRecord;

#define HASH            FnResultHash
#define ENTRY           FnResultRecord
#define PREFIX          fnresultHash
#define KEY(e)          chiStringRef(e->name)
#define EXISTS(e)       e->name
#define KEYEQ(a, b)     chiStringRefEq(a, b)
#define HASHFN          chiHashStringRef
#include "hashtable.h"

#define S_ELEM       FnResultRecord
#define S_LESS(a, b) ((b)->cycles < (a)->cycles)
#include "sort.h"

#define S_ELEM       FFICallRecord
#define S_LESS(a, b) ((b)->cycles < (a)->cycles)
#include "sort.h"

typedef struct {
    const char   *title;
    uint64_t     *count, *cycles;
    double       *cyclesPerCall;
    ChiStringRef *name, *loc;
    uint64_t     totalCount, totalCycles;
    uint32_t     rows;
} FnTable;

static void printFnTable(ChiStats* stats, const FnTable* t) {
    const ChiStatsColumn column[] = {
        { .header = "%",        .type = CHI_STATS_INT,    .ints    = t->cycles,        .sum = t->totalCycles, .percent = true },
        { .header = "cycles",   .type = CHI_STATS_INT,    .ints    = t->cycles,        .sep = true },
        { .header = "%",        .type = CHI_STATS_INT,    .ints    = t->count,         .sum = t->totalCount, .percent = true },
        { .header = "calls",    .type = CHI_STATS_INT,    .ints    = t->count,         .sep = true },
        { .header = "cy/call",  .type = CHI_STATS_FLOAT,  .floats  = t->cyclesPerCall, .sep = true },
        { .header = "function", .type = CHI_STATS_STRING, .strings = t->name,          .sep = true, .left = true },
        { .header = "location", .type = CHI_STATS_STRING, .strings = t->loc,           .left = true },
    };
    chiStatsTable(stats, .title = t->title, .rows = t->rows, .columns = CHI_DIM(column), .data = column);
}

static FnTable allocFnTable(const char* title, uint32_t rows, uint64_t totalCount, uint64_t totalCycles) {
    return (FnTable){
        .title = title,
        .rows = rows,
        .name = chiAllocArr(ChiStringRef, rows),
        .loc = chiAllocArr(ChiStringRef, rows),
        .count = chiAllocArr(uint64_t, rows),
        .cycles = chiAllocArr(uint64_t, rows),
        .cyclesPerCall = chiAllocArr(double, rows),
        .totalCount = totalCount,
        .totalCycles = totalCycles,
    };
}

static void printFnStats(ChiStats* stats, uint32_t rows, fncallData_t* data) {
    HASH_AUTO(FnResultHash, resultHash);

    uint64_t totalCount = 0, totalCycles = 0;
    CHI_STRING_SINK(nameSink);
    HASH_FOREACH(fncallHash, r, &data->hash) {
        ChiLocInfo loc;
        cbyReadLocation(r->code, r->ip, &loc);
        chiLocFmt(nameSink, &loc, CHI_LOCFMT_MODFN | CHI_LOCFMT_FILESEP);
        ChiStringRef name = chiSinkString(nameSink);
        FnResultRecord* fn;
        if (fnresultHashCreate(&resultHash, name, &fn))
            fn->name = chiCStringUnsafeDup(name);
        fn->count += r->count;
        fn->cycles += r->cycles;
        totalCount  += r->count;
        totalCycles += r->cycles;
    }

    sortFnResultRecord(resultHash.entry, resultHash.capacity);

    rows = (uint32_t)CHI_MIN(resultHash.used, rows);
    uint32_t overhead = chiCpuCyclesOverhead();
    FnTable table = allocFnTable("function calls", rows, totalCount, totalCycles);

    size_t i = 0;
    HASH_FOREACH(fnresultHash, r, &resultHash) {
        if (i >= rows)
            break;
        ChiStringRef n = chiStringRef(r->name);
        const uint8_t* p = (const uint8_t*)memchr(n.bytes, '\n', n.size);
        if (p) {
            uint32_t s = (uint32_t)(p - n.bytes);
            table.loc[i].bytes = p + 1;
            table.loc[i].size = n.size - s - 1;
            table.name[i].bytes = n.bytes;
            table.name[i].size = s;
        } else {
            table.name[i] = n;
            table.loc[i] = CHI_EMPTY_STRINGREF;
        }
        table.cycles[i] = r->cycles;
        table.count[i] = r->count;
        table.cyclesPerCall[i] = CHI_MAX((double)r->cycles / (double)r->count - overhead, 0);
        ++i;
    }

    printFnTable(stats, &table);

    HASH_FOREACH(fnresultHash, r, &resultHash)
        chiFree(r->name);
}

static void printFFIStats(ChiStats* stats, uint32_t rows, fficallData_t* data) {
    sortFFICallRecord(data->hash.entry, data->hash.capacity); // after this the hashtable is dysfunctional...

    uint64_t totalCount = 0, totalCycles = 0;
    HASH_FOREACH(fficallHash, r, &data->hash) { // but we can still iterate ;)
        totalCount  += r->count;
        totalCycles += r->cycles;
    }

    rows = (uint32_t)CHI_MIN(data->hash.used, rows);
    uint32_t overhead = chiCpuCyclesOverhead();
    FnTable table = allocFnTable("foreign function calls", rows, totalCount, totalCycles);

    size_t i = 0;
    HASH_FOREACH(fficallHash, r, &data->hash) {
        if (i >= rows)
            break;
        const CbyCode* IP = r->ffi + 4;
        table.name[i] = FETCH_STRING;
        table.loc[i] = CHI_EMPTY_STRINGREF;
        table.cycles[i] = r->cycles;
        table.count[i] = r->count;
        table.cyclesPerCall[i] = CHI_MAX((double)r->cycles / (double)r->count - overhead, 0);
        ++i;
    }

    printFnTable(stats, &table);
}

#endif
