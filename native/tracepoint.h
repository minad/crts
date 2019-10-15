#pragma once

#include "processor.h"

/**
 * All the information associated with a profiling trace point
 */
typedef struct {
    ChiWorker*    worker; ///< Current worker
    ChiProcessor* proc;   ///< Current processor (can be 0)
    Chili*        frame;  ///< Current stack pointer (can be 0)
    ChiCont       cont;   ///< Current continuation (can be 0)
    const char*   name;   ///< Tracepoint name (can be 0)
    size_t        alloc;  ///< Should be != 0 for allocation tracepoint
    bool          thread; ///< Add thread frame to trace
} ChiTracePoint;

_Static_assert(!CHI_TRACEPOINTS_CONT_ENABLED || CHI_TRACEPOINTS_ENABLED, "Tracepoints must be enabled.");

void chiTraceHandler(const ChiTracePoint*);

#if CHI_TRACEPOINTS_ENABLED
CHI_INL int32_t chiTraceTriggerGet(ChiWorker* w) { return atomic_load_explicit(w->tp, memory_order_relaxed); }
CHI_INL void chiTraceTriggerSet(ChiWorker* w, int32_t x) { atomic_store_explicit(w->tp, x, memory_order_relaxed); }
#else
CHI_INL int32_t chiTraceTriggerGet(ChiWorker* CHI_UNUSED(w)) { return 0; }
CHI_INL void chiTraceTriggerSet(ChiWorker* CHI_UNUSED(w), int32_t CHI_UNUSED(x)) {}
#endif

CHI_INL bool chiTraceTimeTriggered(ChiWorker* w) {
    return CHI_UNLIKELY(chiTraceTriggerGet(w) > 0);
}

CHI_INL bool chiTraceAllocTriggered(ChiWorker* w) {
    return CHI_UNLIKELY(chiTraceTriggerGet(w) < 0);
}

/**
 * Tracepoint used for profiling
 */
#define CHI_TTW(w, ...)                                                 \
    ({                                                                  \
        ChiWorker* _w = (w);                                            \
        if (chiTraceTimeTriggered(_w))                                  \
            chiTraceHandler(&(ChiTracePoint){ .worker = _w, ##__VA_ARGS__ }); \
    })

#define CHI_TTP(p, ...)                                 \
    ({                                                  \
        ChiProcessor* _p = (p);                         \
        CHI_TTW(_p->worker, .proc = _p, ##__VA_ARGS__); \
    })
