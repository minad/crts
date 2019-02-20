#include <chili/cont.h>

#if CBY_BACKEND_CYCLES_ENABLED

#include "bytecode/opcodes.h"
#include "instrument/append.h"
#include "instrument/count.h"
#include "instrument/cycles.h"

INSTRUMENT_APPEND(countcycles, count, cycles)

CHI_INL void _countcyclesDestroy(CbyInterpreter* interp, countcyclesData_t* data) {
    cbyPrintInsnTable(interp, &data->a, &data->b);
    countcyclesDestroy(interp, data);
}
#define countcyclesDestroy _countcyclesDestroy

#define INSTRUMENT_NAME countcycles
#define INTERP_NAME     cycles
#include "interp.h"

#endif
