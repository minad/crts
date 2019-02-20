#include "json.h"

#define TE_COUNTER(...) CHECK(teEventCounter, e, __VA_ARGS__)
#define TE_NAME(...)    CHECK(teEventName, __VA_ARGS__)
#define TE_BLOCK(...)   CHECK(teEventBlock, __VA_ARGS__)

static const char* teColor(ChiEvent e) {
    CHI_WARN_OFF(switch-enum)
    switch (e) {
    case CHI_EVENT_THREAD_RUN_END:   return "thread_state_running";
    case CHI_EVENT_PROC_SUSPEND_END: return "thread_state_sleeping";
    case CHI_EVENT_PROC_SYNC_END:    return "yellow";
    case CHI_EVENT_GC_SLICE_END:     return "terrible";
    case CHI_EVENT_THREAD_SCHED_END: return "bad";
    default: return 0;
    }
    CHI_WARN_ON
}

static CHI_WU bool teEventName(Log* log, uint64_t tid, const char* prefix, ChiStringRef name) {
    XINIT;
    XUBLOCK_BEGIN("e");
    XFIELD("name", SAFE_QSTR("thread_name"));
    XFIELD("ph",   SAFE_QSTR("M"));
    XFIELD("pid",  CHAR('0'));
    XFIELD("tid",  NUM(tid));
    XBLOCK_BEGIN("args");
    XFIELD("name", CHAR('"'); RAWSTR(prefix); CHECK(jsonEscape, name); CHAR('"'));
    XBLOCK_END("args");
    XUBLOCK_END("e");
    CHAR('\n');

    XUBLOCK_BEGIN("e");
    XFIELD("name", SAFE_QSTR("thread_sort_index"));
    XFIELD("ph",   SAFE_QSTR("M"));
    XFIELD("pid",  CHAR('0'));
    XFIELD("tid",  NUM(tid));
    XBLOCK_BEGIN("args");
    XFIELD("sort_index", NUM(tid));
    XBLOCK_END("args");
    XUBLOCK_END("e");
    CHAR('\n');

    return true;
}

static CHI_WU bool teMicros(Log* log, ChiNanos time) {
    NUM(CHI_UN(Nanos, time) / 1000); CHAR('.');
    uint64_t frac = CHI_UN(Nanos, time) % 1000;
    if (frac < 100) CHAR('0');
    if (frac < 10) CHAR('0');
    NUM(frac);
    return true;
}

static CHI_WU bool teEventBlock(Log* log, EventContext ctx, const Event* e) {
    Chili threadName = ctx == CTX_PROCESSOR && eventDesc[e->type].ctx == CTX_THREAD
        ? chiThreadName(e->thread) : CHI_FALSE;
    XINIT;
    XEVENT_JSON_BEGIN(e->type);
    XUBLOCK_BEGIN("payload");
    XFIELD("name",
           CHAR('"'); RAWSTR(chiEventName[e->type]);
           if (e->type == CHI_EVENT_GC_SCAVENGER_END)
               NUM(e->data.GC_SCAVENGER_END->gen);
           if (chiTrue(threadName)) {
               CHAR(' ');
               CHECK(jsonEscape, chiStringRef(&threadName));
           }
           CHAR('"'));
    XFIELD("ts", teMicros(log, e->time));
    if (eventDesc[e->type].cls == CLASS_END)
        XFIELD("dur", teMicros(log, e->dur));
    XFIELD("ph",  CHAR('"'); CHAR("BXi"[eventDesc[e->type].cls]); CHAR('"'));
    XFIELD("pid", CHAR('0'));
    XFIELD("tid",
           switch (ctx) {
           case CTX_THREAD:
               NUM(!chiTrue(chiThreadName(e->thread))
                   ? CHI_EVENT_TID_UNNAMED
                   : CHI_EVENT_TID_THREAD + chiToUnboxed(chiToThread(e->thread)->tid));
               break;
           case CTX_PROCESSOR:
           case CTX_WORKER:
               NUM(CHI_EVENT_TID_WORKER + e->wid);
               break;
           case CTX_RUNTIME:
               NUM(CHI_EVENT_TID_RUNTIME);
               break;
           });
    const char* color = teColor(e->type);
    if (color)
        XFIELD("cname", SAFE_QSTR(color));
    if (e->data.ANY || chiTrue(threadName)) {
        XBLOCK_BEGIN("args");
        if (e->data.ANY)
            jsonPayload(log, e, xstate);
        if (chiTrue(threadName))
            XFIELD("thread", XSTR(chiStringRef(&threadName)));
        XBLOCK_END("args");
    }
    XUBLOCK_END("payload");
    CHAR('\n');
    XEVENT_JSON_END(e->type);
    return true;
}

static CHI_WU bool teEventCounter(Log* log, const Event* e, const char* name, uint64_t val) {
    XINIT;
    XUBLOCK_BEGIN("e");
    XFIELD("name", SAFE_QSTR(name));
    XFIELD("ph",   SAFE_QSTR("C"));
    XFIELD("pid",  CHAR('0'));
    XFIELD("tid",  NUM(CHI_EVENT_TID_RUNTIME));
    XFIELD("ts", teMicros(log, e->time));
    XBLOCK_BEGIN("args");
    XFIELD("v", NUM(val));
    XBLOCK_END("args");
    XUBLOCK_END("e");
    CHAR('\n');
    return true;
}

// See https://github.com/catapult-project/catapult/blob/master/docs/trace-event-format.md
static CHI_WU bool teEvent(Log* log, const Event* e) {
    if (e->type == CHI_EVENT_THREAD_NAME) {
        TE_NAME(CHI_EVENT_TID_THREAD + e->data.THREAD_NAME->tid, "thread ", e->data.THREAD_NAME->name);
    } else if (e->type == CHI_EVENT_WORKER_NAME) {
        TE_NAME(CHI_EVENT_TID_WORKER + e->wid, "", e->data.WORKER_NAME->name);
    } else if (eventDesc[e->type].ctx == CTX_THREAD) {
        if (eventDesc[e->type].cls != CLASS_INSTANT)
            TE_BLOCK(CTX_PROCESSOR, e); // duplicate block
        TE_BLOCK(CTX_THREAD, e);
    } else {
        TE_BLOCK(eventDesc[e->type].ctx, e);
    }
    CHI_WARN_OFF(switch-enum)
    switch (e->type) {
    case CHI_EVENT_BEGIN:
        TE_NAME(CHI_EVENT_TID_RUNTIME, "runtime", CHI_EMPTY_STRINGREF);
        TE_NAME(CHI_EVENT_TID_UNNAMED, "unnamed thread", CHI_EMPTY_STRINGREF);
        break;
    case CHI_EVENT_GC_MARK_STATS:
        TE_COUNTER("scanned objects", e->data.GC_MARK_STATS->object.count);
        TE_COUNTER("scanned stacks", e->data.GC_MARK_STATS->stack.count);
        break;
    case CHI_EVENT_GC_SCAVENGER_END:
        {
            const ChiEventScavenger* scav = e->data.GC_SCAVENGER_END;
            TE_COUNTER("scavenger promoted words",   scav->object.promoted.words);
            TE_COUNTER("scavenger promoted objects", scav->object.promoted.count);
            TE_COUNTER("scavenger copied words",     scav->object.copied.words);
            TE_COUNTER("scavenger copied objects",   scav->object.copied.count);
            TE_COUNTER("scavenger promoted raw words",   scav->raw.promoted.words);
            TE_COUNTER("scavenger promoted raw objects", scav->raw.promoted.count);
            TE_COUNTER("scavenger copied raw words",     scav->raw.copied.words);
            TE_COUNTER("scavenger copied raw objects",   scav->raw.copied.count);
            break;
        }
    case CHI_EVENT_ACTIVITY:
        TE_COUNTER("resident size", e->data.ACTIVITY->residentSize);
        break;
    case CHI_EVENT_STACK_GROW:
    case CHI_EVENT_STACK_SHRINK:
        TE_COUNTER("new stack size", e->data.STACK_GROW->newSize);
        break;
    default:
        break;
    }
    CHI_WARN_ON
    return true;
}
#include "undef.h"
