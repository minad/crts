#pragma once

#include "option.h"
#include "event/defs.h"

#define CHI_FOREACH_GCMODE(MODE, SEP) \
    MODE(NONE, none) SEP \
    MODE(NOMS, noms) SEP \
    CHI_IF(CHI_GC_CONC_ENABLED, MODE(CONC, conc) SEP) \
    MODE(INC, inc) SEP \
    MODE(FULL, full)
#define _CHI_GCMODE(N, n) CHI_GC_##N,
typedef enum { CHI_FOREACH_GCMODE(_CHI_GCMODE,) } ChiGCMode;
#undef _CHI_GCMODE

#define CHI_FOREACH_EVENTFORMAT(FORMAT, SEP)                            \
    FORMAT(NONE, none, none)                                            \
        CHI_IF(CHI_EVENT_CSV_ENABLED,    SEP FORMAT(CSV,    csv,    csv)) \
        CHI_IF(CHI_EVENT_JSON_ENABLED,   SEP FORMAT(JSON,   json,   json)) \
        CHI_IF(CHI_EVENT_MP_ENABLED,     SEP FORMAT(MP,     mp,     mp)) \
        CHI_IF(CHI_EVENT_PRETTY_ENABLED, SEP FORMAT(PRETTY, pretty, log)) \
        CHI_IF(CHI_EVENT_TE_ENABLED,     SEP FORMAT(TE,     te,     json)) \
        CHI_IF(CHI_EVENT_XML_ENABLED,    SEP FORMAT(XML,    xml,    xml))
#define _CHI_EVENTFORMAT(N, n, e) CHI_EVENTFORMAT_##N,
typedef enum { CHI_FOREACH_EVENTFORMAT(_CHI_EVENTFORMAT,) } ChiEventFormat;
#undef _CHI_EVENTFORMAT

#define CHI_FOREACH_BH(FORMAT, SEP)       \
    FORMAT(LAZY,  lazy)  SEP              \
    FORMAT(EAGER, eager) SEP              \
    FORMAT(NONE,  none)
#define _CHI_BH(N, n) CHI_BH_##N,
typedef enum { CHI_FOREACH_BH(_CHI_BH,) } ChiBlackhole;
#undef _CHI_BH

#define CHI_FOREACH_COLOR(FORMAT, SEP)        \
    FORMAT(AUTO, a) SEP                       \
    FORMAT(ON,   t) SEP                       \
    FORMAT(OFF,  f)
#define _CHI_SINK_COLOR(N, n) CHI_SINK_COLOR_##N,
typedef enum { CHI_FOREACH_COLOR(_CHI_SINK_COLOR,) CHI_SINK_BINARY } ChiSinkColor;
#undef _CHI_SINK_COLOR

typedef struct {
    struct {
        ChiGCMode mode;
        struct {
            ChiMicros   slice;
            uint32_t    multiplicity;
            bool        noPromote;
            bool        noCollapse;
        } scav;
        struct {
            ChiMicros   slice;
            bool        noCollapse;
        } ms;
        struct {
            uint32_t    sweepers;
            uint32_t    markers;
        } conc[CHI_GC_CONC_ENABLED];
    } gc;
    struct {
        uint32_t    gen;
        uint32_t    nursery;
        size_t      size;
        size_t      chunk;
    } block;
    struct {
        CHI_IF(CHI_HEAP_CHECK_ENABLED, bool check;)
        CHI_IF(CHI_HEAP_PROF_ENABLED, uint32_t prof;)
        uint32_t    scanDepth;
        struct {
            size_t   size;
            uint32_t alloc;
            uint32_t soft;
            uint32_t hard;
        } limit;
        struct {
            size_t   max;
            size_t   chunk;
            uint32_t sub;
        } small, medium;
    } heap;
    struct {
        uint32_t    init;
        uint32_t    max;
        uint32_t    growth;
        uint32_t    trace;
        bool        traceMangled;
    } stack;
    struct {
        uint32_t         bufSize;
        uint8_t          filter[CHI_EVENT_FILTER_SIZE];
        ChiEventFormat   format;
        char             file[64];
        size_t           msgSize;
    } event[CHI_EVENT_ENABLED];
    struct {
        bool        json;
        bool        cumulative;
        char        file[64];
        uint32_t    enabled;
        uint32_t    tableCell;
        uint32_t    tableRows;
        uint32_t    bins;
        uint32_t    scale;
    } stats[CHI_STATS_ENABLED];
    ChiSinkColor    color;
    bool            fastExit;
    uint32_t        interruptLimit;
    uint32_t        processors;
    uint32_t        evalCount;
    ChiBlackhole    blackhole;
    ChiMillis       interval;
} ChiRuntimeOption;

extern const ChiOption chiRuntimeOptionList[];
extern const ChiOption chiProfOptionList[];
extern const ChiOption chiChunkOptionList[];

void chiRuntimeOptionDefaults(ChiRuntimeOption*);
void chiRuntimeOptionValidate(ChiRuntimeOption*);
