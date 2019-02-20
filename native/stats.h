#pragma once

#include <chili/object/string.h>
#include "rtoption.h"

typedef enum {
    CHI_STATS_FLOAT,
    CHI_STATS_INT,
    CHI_STATS_STRING,
} ChiStatsType;

typedef struct {
    const char*       header;
    uint64_t          sum;
    union {
        uint64_t*     ints;
        double*       floats;
        ChiStringRef* strings;
    };
    ChiStatsType      type       : 2;
    bool              left       : 1;
    bool              sep        : 1;
    bool              percent    : 1;
    bool              cumulative : 1;
} ChiStatsColumn;

typedef struct {
    const char*           title;
    uint32_t              rows;
    uint32_t              columns;
    const ChiStatsColumn* data;
} ChiStatsTable;

typedef struct ChiSink_ ChiSink;

typedef struct {
    ChiSink* sink;
    uint32_t state;
    uint32_t cell;
    bool     json;
} ChiStats;

#if CHI_STATS_ENABLED
void chiStatsSetup(ChiStats*, uint32_t, bool);
void chiStatsDestroy(ChiStats*, const char*, ChiSinkColor);
void chiStatsTitle(ChiStats*, const char*);
void chiStatsRow(ChiStats*, const char*);
void chiStatsAddTable(ChiStats*, const ChiStatsTable*);
void chiStatsBytesPerSec(ChiStats*, const char*, double);
void chiStatsBytes(ChiStats*, const char*, uint64_t);
void chiStatsTime(ChiStats*, const char*, ChiNanos);
void chiStatsString(ChiStats*, const char*, ChiStringRef);
void chiStatsIntUnit(ChiStats*, const char*, uint64_t, bool, const char*);
void chiStatsFloatUnit(ChiStats*, const char*, double, bool, const char*);
#else
CHI_INL void chiStatsSetup(ChiStats* CHI_UNUSED(s), uint32_t CHI_UNUSED(cell), bool CHI_UNUSED(json)) {}
CHI_INL void chiStatsDestroy(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(file), ChiSinkColor CHI_UNUSED(color)) {}
CHI_INL void chiStatsTitle(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n)) {}
CHI_INL void chiStatsRow(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n)) {}
CHI_INL void chiStatsAddTable(ChiStats* CHI_UNUSED(s), const ChiStatsTable* CHI_UNUSED(t)) {}
CHI_INL void chiStatsBytesPerSec(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n), double CHI_UNUSED(v)) {}
CHI_INL void chiStatsBytes(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n), uint64_t CHI_UNUSED(v)) {}
CHI_INL void chiStatsTime(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n), ChiNanos CHI_UNUSED(v)) {}
CHI_INL void chiStatsString(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n), ChiStringRef CHI_UNUSED(v)) {}
CHI_INL void chiStatsIntUnit(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n), uint64_t CHI_UNUSED(v), bool CHI_UNUSED(e), const char* CHI_UNUSED(u)) {}
CHI_INL void chiStatsFloatUnit(ChiStats* CHI_UNUSED(s), const char* CHI_UNUSED(n), double CHI_UNUSED(v), bool CHI_UNUSED(e), const char* CHI_UNUSED(u)) {}
#endif

CHI_INL void chiStatsInt(ChiStats* s, const char* n, uint64_t v) {
    chiStatsIntUnit(s, n, v, false, 0);
}

CHI_INL void chiStatsFloat(ChiStats* s, const char* n, double v) {
    chiStatsFloatUnit(s, n, v, false, 0);
}

CHI_INL void chiStatsPercent(ChiStats* s, const char* n, double v) {
    chiStatsFloatUnit(s, n, 100 * v, false, "%");
}

#define chiStatsTable(stats, ...)                                       \
    (chiStatsAddTable((stats), &(ChiStatsTable){ __VA_ARGS__ }))
