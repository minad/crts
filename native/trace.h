#pragma once

#include "processor.h"

/**
 * All the information associated with a profiling trace point
 */
typedef struct {
    Chili*      frame;  ///< Current stack pointer (can be 0)
    ChiCont     cont;   ///< Current continuation (can be 0)
    const char* name;   ///< Tracepoint name (can be 0)
    size_t      alloc;  ///< Should be != 0 for allocation tracepoint
    bool        thread; ///< Add thread frame to trace
} ChiTracePoint;

#if CHI_TRACEPOINTS_ENABLED
void chiTraceHandler(ChiWorker*, const ChiTracePoint*);
#else
CHI_WEAK void chiTraceHandler(ChiWorker*, const ChiTracePoint*);
#endif

CHI_INL bool chiTraceTimeTriggered(ChiWorker* worker) {
    return CHI_UNLIKELY(*(worker)->tp > 0);
}

CHI_INL bool chiTraceAllocTriggered(ChiWorker* worker) {
    return CHI_UNLIKELY(*(worker)->tp < 0);
}

/**
 * Tracepoint used for profiling
 */
#define CHI_TT(w, ...)                                                  \
    ({                                                                  \
        ChiWorker* _w = (w);                                            \
        void* handlerExists = chiTraceHandler; /* Avoid warning */      \
        if ((CHI_TRACEPOINTS_ENABLED || handlerExists) && chiTraceTimeTriggered(_w)) \
            chiTraceHandler(_w, &(ChiTracePoint){ __VA_ARGS__ });       \
    })
