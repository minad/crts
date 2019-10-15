#include "fnlog.h"

#if CBY_BACKEND_FNLOG_ENABLED

#include "../bytecode/decode.h"
#include "native/event.h"

#define FNLOG_EVENT(e, fn)                              \
    ({                                                  \
        if (chiEventEnabled(proc, FNLOG_##e)) {         \
            ChiEventFnLog event;                        \
            cbyReadFnLocation(fn, &event);              \
            chiEventStruct(proc, FNLOG_##e, &event);    \
        }                                               \
    })

void _fnlogEnter(ChiProcessor* proc, Chili fn, bool jmp) {
    if (jmp)
        FNLOG_EVENT(ENTER_JMP, fn);
    else
        FNLOG_EVENT(ENTER, fn);
}

void _fnlogLeave(ChiProcessor* proc, Chili fn) {
    FNLOG_EVENT(LEAVE, fn);
}

void _fnlogFFI(ChiProcessor* proc, const CbyCode* ffi) {
    if (chiEventEnabled(proc, FNLOG_FFI)) {
        const CbyCode* IP = ffi + 4;
        ChiStringRef name = FETCH_STRING;
        chiEvent(proc, FNLOG_FFI, .name = name);
    }
}

#endif
