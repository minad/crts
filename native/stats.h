#pragma once

#include "rtoption.h"

typedef enum {
    CHI_STATS_FLOAT,
    CHI_STATS_INT,
    CHI_STATS_STRING,
    CHI_STATS_PATH,
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

CHI_INTERN void chiStatsSetup(ChiStats*, uint32_t, bool);
CHI_INTERN void chiStatsDestroy(ChiStats*, const char*, ChiSinkColor);
CHI_INTERN void chiStatsTitle(ChiStats*, const char*);
CHI_INTERN void chiStatsRow(ChiStats*, const char*);
CHI_INTERN void chiStatsAddTable(ChiStats*, const ChiStatsTable*);
CHI_INTERN void chiStatsBytesPerSec(ChiStats*, const char*, double);
CHI_INTERN void chiStatsBytes(ChiStats*, const char*, uint64_t);
CHI_INTERN void chiStatsTime(ChiStats*, const char*, ChiNanos);
CHI_INTERN void chiStatsString(ChiStats*, const char*, ChiStringRef);
CHI_INTERN void chiStatsIntUnit(ChiStats*, const char*, uint64_t, bool, const char*);
CHI_INTERN void chiStatsFloatUnit(ChiStats*, const char*, double, bool, const char*);

CHI_INL void chiStatsInt(ChiStats* s, const char* n, uint64_t v) {
    chiStatsIntUnit(s, n, v, false, 0);
}

CHI_INL void chiStatsFloat(ChiStats* s, const char* n, double v) {
    chiStatsFloatUnit(s, n, v, false, 0);
}

CHI_INL void chiStatsPercent(ChiStats* s, const char* n, double v) {
    chiStatsFloatUnit(s, n, 100 * v, false, "%");
}

CHI_INL void chiStatsPerSec(ChiStats* s, const char* n, double v) {
    chiStatsFloatUnit(s, n, v, false, "/s");
}

CHI_INL void chiStatsWordsPerSec(ChiStats* s, const char* n, double v) {
    chiStatsBytesPerSec(s, n, CHI_WORDSIZE * v);
}

CHI_INL void chiStatsWords(ChiStats* s, const char* n, uint64_t v) {
    chiStatsBytes(s, n, CHI_WORDSIZE * v);
}

#define chiStatsTable(stats, ...)                                       \
    (chiStatsAddTable((stats), &(ChiStatsTable){ __VA_ARGS__ }))
