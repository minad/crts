#pragma once

#include "event/utils.h"
#include "bit.h"

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

#define _CHI_EVENT_FILTER_ENABLED(ctx, type)                            \
    (CHI_EVENT_ENABLED &&                                               \
     CHI_UNLIKELY(chiBitGet(_CHI_EVENT_RT(ctx)->option.event->filter, CHI_EVENT_##type)))

#define CHI_EVENT_P(ctx, type)                                          \
    (_CHI_EVENT_FILTER_ENABLED(ctx, type) || _CHI_DTRACE_ENABLED(type) || _CHI_LTTNG_ENABLED(type))

#define CHI_EVENT_STRUCT(ctx, type, event)                              \
    ({                                                                  \
        _CHI_EVENT_CTX_##type* _ctx = (ctx);                            \
        CHI_FIELD_TYPE(ChiEventData, type) _eventptr = (event);         \
        if (_CHI_EVENT_FILTER_ENABLED(_ctx, type))                      \
            chiEvent(_ctx, CHI_EVENT_##type, (ChiEventData){.type=_eventptr}); \
        _CHI_DTRACE(type, _ctx, _eventptr);                             \
        _CHI_LTTNG(type, _ctx, _eventptr);                              \
    })

#define CHI_EVENT(ctx, type, arg, ...)                                  \
    CHI_EVENT_STRUCT(ctx, type, &((typeof(*(CHI_FIELD_TYPE(ChiEventData, type))0)){ arg, __VA_ARGS__ }))

#define CHI_EVENT0(ctx, type)                                           \
    ({                                                                  \
        _CHI_EVENT_CTX_##type* _ctx = (ctx);                            \
        if (_CHI_EVENT_FILTER_ENABLED(_ctx, type))                      \
            chiEvent(_ctx, CHI_EVENT_##type, (ChiEventData){0});        \
        _CHI_DTRACE(type, _ctx);                                        \
        _CHI_LTTNG(type, _ctx);                                         \
    })

extern const char* const chiEventName[];

#if CHI_EVENT_ENABLED
void chiEventSetup(ChiRuntime*);
void chiEventDestroy(ChiRuntime*);
void chiEventWorkerStart(ChiWorker*);
void chiEventWorkerStop(ChiWorker*);
void chiEvent(void*, ChiEvent, ChiEventData);
CHI_WU bool chiEventModifyFilter(uint8_t*, ChiStringRef);
#else
CHI_INL void chiEventSetup(ChiRuntime* CHI_UNUSED(rt)) {}
CHI_INL void chiEventDestroy(ChiRuntime* CHI_UNUSED(rt)) {}
CHI_INL void chiEventWorkerStart(ChiWorker* CHI_UNUSED(worker)) {}
CHI_INL void chiEventWorkerStop(ChiWorker* CHI_UNUSED(worker)) {}
CHI_INL void chiEvent(void* CHI_UNUSED(ctx), ChiEvent CHI_UNUSED(e), ChiEventData CHI_UNUSED(d)) {}
CHI_INL CHI_WU bool chiEventModifyFilter(uint8_t* CHI_UNUSED(f), ChiStringRef CHI_UNUSED(s)) { return true; }
#endif
