#pragma once

#include "option.h"

#define CHI_DEFAULT_PROF_OPTION  { .rate = 100, .maxDepth = 32, .maxStacks = 10000 }

/**
 * Profiler options. Kept in separate datastructure
 * since we can use the profiler even if the runtime
 * is compiled without profiling support. The interpreter
 * always includes the profiler. In the case where
 * the runtime is compiled without profiling support,
 * the profile only includes measures of the interpreted functions.
 */
typedef struct {
    uint32_t rate, maxDepth, maxStacks;
    char     file[64];
    bool     flat, alloc, off;
} ChiProfOption;

typedef struct ChiRuntime_ ChiRuntime;

#if CHI_PROF_ENABLED
CHI_INTERN void chiProfilerSetup(ChiRuntime*, const ChiProfOption*);
CHI_EXTERN ChiOptionGroup chiProfOptions;
#else
void chiProfilerSetup(ChiRuntime*, const ChiProfOption*);
#endif
