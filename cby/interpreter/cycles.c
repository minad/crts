#include <chili/cont.h>

#if CBY_BACKEND_CYCLES_ENABLED

#include "../bytecode/opcodes.h"
#include "../instrument/append.h"
#include "../instrument/count.h"
#include "../instrument/cycles.h"

INSTRUMENT_APPEND(countcycles, count, cycles)

void insnStats(CbyInterpreter*, const countData*, const cyclesData*);

CHI_INL void _countcyclesDestroy(CbyInterpreter* interp, countcyclesData* data) {
    insnStats(interp, &data->a, &data->b);
    countcyclesDestroy(interp, data);
}

#define countcyclesDestroy _countcyclesDestroy

#ifdef __clang__
// we use switch since this leads to smaller code being generated
#define INTERP_DISPATCH    switch
#endif

#define INSTRUMENT_NAME    countcycles
#define INTERP_NAME        cycles
#include "interpreter.h"

#endif
