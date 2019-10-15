#pragma once

#include "event/defs.h"
#include "option.h"

#define CHI_FOREACH_GCMODE(MODE, SEP) \
    MODE(NONE, none) SEP \
    CHI_IF(CHI_GC_CONC_ENABLED, MODE(CONC, conc) SEP) \
    MODE(INC, inc)
#define _CHI_GCMODE(N, n) CHI_GC_##N,
typedef enum { CHI_FOREACH_GCMODE(_CHI_GCMODE,) } ChiGCMode;
#undef _CHI_GCMODE

#define CHI_FOREACH_SINK_COLOR(FORMAT, SEP)   \
    FORMAT(AUTO, a) SEP                       \
    FORMAT(ON,   t) SEP                       \
    FORMAT(OFF,  f)
#define _CHI_SINK_COLOR(N, n) CHI_SINK_COLOR_##N,
typedef enum { CHI_FOREACH_SINK_COLOR(_CHI_SINK_COLOR,) CHI_SINK_BINARY, CHI_SINK_TEXT=CHI_SINK_COLOR_ON } ChiSinkColor;
#undef _CHI_SINK_COLOR

typedef struct {
    ChiMillis       interval;
    struct {
        struct {
            uint32_t    aging;
            bool        noCollapse;
        } scav;
        struct {
            ChiMicros   slice;
            ChiGCMode   mode;
            CHI_IF(CHI_GC_CONC_ENABLED, uint32_t worker;)
            bool        noCollapse;
        } major;
    } gc;
    struct {
        size_t      size;
        size_t      chunk;
        uint32_t    nursery;
    } block;
    struct {
        struct {
            size_t   init;
            size_t   max;
            uint32_t full;
            uint32_t soft;
            uint32_t hard;
        } limit;
        struct {
            size_t   segment;
            size_t   page;
        } small, medium;
        uint32_t    scanDepth;
    } heap;
    struct {
        uint32_t    init;
        uint32_t    max;
        uint32_t    growth;
        uint32_t    trace;
        bool        traceMangled;
        bool        traceCycles;
    } stack;
    CHI_IF(CHI_EVENT_ENABLED, struct {
        uintptr_t        filter[_CHI_EVENT_FILTER_SIZE];
        size_t           msgSize;
        uint32_t         bufSize;
        char             file[64];
        bool             enabled;
        CHI_IF(CHI_EVENT_PRETTY_ENABLED, bool pretty;)
    } event;)
    CHI_IF(CHI_STATS_ENABLED, struct {
        uint32_t    tableCell;
        uint32_t    tableRows;
        uint32_t    bins;
        uint32_t    scale;
        char        file[64];
        bool        json;
        bool        cumulative;
        bool        enabled;
        bool        verbose;
    } stats;)
    CHI_IF(CHI_COLOR_ENABLED, ChiSinkColor color;)
    CHI_IF(CHI_SYSTEM_HAS_TASK, uint32_t processors;)
} ChiRuntimeOption;

CHI_EXTERN const ChiOption chiRuntimeOptionList[];
CHI_INTERN void chiRuntimeOptionDefaults(ChiRuntimeOption*);
CHI_INTERN void chiRuntimeOptionValidate(ChiRuntimeOption*);
