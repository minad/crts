#include <chili/cont.h>

#if CBY_BACKEND_CALLS_ENABLED

#include "../bytecode/opcodes.h"
#include "../instrument/append.h"
#include "../instrument/fficall.h"
#include "../instrument/fncall.h"
#include "native/location.h"
#include "native/sink.h"

INSTRUMENT_APPEND(calls, fncall, fficall)

void callsStats(CbyInterpreter*, fncallData*, fficallData*);

CHI_INL void _callsDestroy(CbyInterpreter* interp, callsData* data) {
    callsStats(interp, &data->a, &data->b);
    callsDestroy(interp, data);
}

#define callsDestroy _callsDestroy
#define INTERP_NAME  calls
#include "interpreter.h"

#endif
