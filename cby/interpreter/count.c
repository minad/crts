#include <chili/cont.h>

#if CBY_BACKEND_COUNT_ENABLED

#include "../bytecode/opcodes.h"
#include "../instrument/count.h"
#include "../instrument/cycles.h"

void insnStats(CbyInterpreter*, const countData*, const cyclesData*);

CHI_INL void _countDestroy(CbyInterpreter* interp, countData* data) {
    insnStats(interp, data, 0);
    countDestroy(interp, data);
}

#define countDestroy _countDestroy

#define INTERP_NAME count
#include "interpreter.h"

#endif
