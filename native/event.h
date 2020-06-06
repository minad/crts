#pragma once

#include "system.h"

#if CHI_DTRACE_ENABLED
#  include "event/dtrace.h"
#  define _CHI_DTRACE_ENABLED(type)  CHI_UNLIKELY(CHILI_##type##_ENABLED())
#  define _CHI_DTRACE(type, ...)     CHILI_##type(__VA_ARGS__)
#else
#  define _CHI_DTRACE_ENABLED(type)  0
#  define _CHI_DTRACE(type, ...)     ({})
#endif

#if CHI_LTTNG_ENABLED
#  include "event/lttng.h"
#  define _CHI_LTTNG_ENABLED(type)   CHI_UNLIKELY(tracepoint_enabled(chili, type))
#  define _CHI_LTTNG(type, ...)      tracepoint(chili, type, ##__VA_ARGS__)
#else
#  define _CHI_LTTNG_ENABLED(type)   0
#  define _CHI_LTTNG(type, ...)     ({})
#endif

// C11 _Generic could be used here instead, but it is incompatible with Coverity and Sparse
/*
#define _CHI_EVENT_RT(ctx)                                      \
    _Generic((ctx),                                             \
             ChiProcessor* : ((ChiProcessor*)(ctx))->rt,        \
             ChiWorker*    : ((ChiWorker*)(ctx))->rt,           \
             ChiRuntime*   : (ctx))
*/
#define _CHI_EVENT_RT(ctx)                                              \
    __builtin_choose_expr(__builtin_types_compatible_p(typeof(ctx), ChiProcessor*), ((ChiProcessor*)(void*)(ctx))->rt, \
    __builtin_choose_expr(__builtin_types_compatible_p(typeof(ctx), ChiWorker*), ((ChiWorker*)(void*)(ctx))->rt, ctx))

#define _CHI_EVENT_FILTER_ENABLED(ctx, type) \
    CHI_AND(CHI_EVENT_ENABLED, chiEventFilterEnabled(_CHI_EVENT_RT(ctx)->option.event.filter, CHI_EVENT_##type))

#define chiEventEnabled(ctx, type)                                          \
    (_CHI_EVENT_FILTER_ENABLED(ctx, type) || _CHI_DTRACE_ENABLED(type) || _CHI_LTTNG_ENABLED(type))

#define _chiEventStruct(ctx, type, ...)                                 \
    ({                                                                  \
        typeof(ctx) _ctx = (ctx);                                       \
        if (_CHI_EVENT_FILTER_ENABLED(_ctx, type))                      \
            _CHI_EVENT_##type(_ctx, ##__VA_ARGS__);                     \
        _CHI_DTRACE(type, _ctx, ##__VA_ARGS__);                         \
        _CHI_LTTNG(type, _ctx, ##__VA_ARGS__);                          \
    })

#define chiEventStruct(ctx, type, event)                                \
    ({ typeof(event) _event = (event); _chiEventStruct((ctx), type, _event); })
#define chiEvent(ctx, type, arg, ...)                                   \
    chiEventStruct(ctx, type, &((typeof(*(CHI_FIELD_TYPE(ChiEventPayload, type)*)0)){ arg, __VA_ARGS__ }))
#define chiEvent0(ctx, type) _chiEventStruct((ctx), type)

#define CHI_FOREACH_EVENT_FORMAT(FORMAT, SEP)    \
    FORMAT(NONE,   none)   SEP                   \
    FORMAT(PRETTY, pretty) SEP                   \
    FORMAT(JSON,   json)   SEP                   \
    FORMAT(CSV,    csv)    SEP                   \
    FORMAT(BIN,    evlog)
#define _CHI_EVENT_FORMAT(N, n) CHI_EVFMT_##N,
typedef enum { CHI_FOREACH_EVENT_FORMAT(_CHI_EVENT_FORMAT,) } ChiEventFormat;
#undef _CHI_EVENT_FORMAT

#if CHI_EVENT_ENABLED
CHI_EXTERN const char* const chiEventName[];
CHI_INTERN void chiEventSetup(ChiRuntime*);
CHI_INTERN void chiEventDestroy(ChiRuntime*);
CHI_INTERN void chiEventWorkerStart(ChiWorker*);
CHI_INTERN void chiEventWorkerStop(ChiWorker*);
CHI_INTERN CHI_WU bool chiEventModifyFilter(void*, ChiStringRef);

#if CHI_EVENT_CONVERT_ENABLED
typedef struct ChiSink_ ChiSink;
CHI_INTERN void chiEventConvert(const char*, ChiFile, ChiSink*, ChiEventFormat, size_t);
#endif
#else
CHI_INL void chiEventSetup(ChiRuntime* CHI_UNUSED(rt)) {}
CHI_INL void chiEventDestroy(ChiRuntime* CHI_UNUSED(rt)) {}
CHI_INL void chiEventWorkerStart(ChiWorker* CHI_UNUSED(worker)) {}
CHI_INL void chiEventWorkerStop(ChiWorker* CHI_UNUSED(worker)) {}
CHI_INL CHI_WU bool chiEventModifyFilter(void* CHI_UNUSED(f), ChiStringRef CHI_UNUSED(s)) { return true; }
#endif
