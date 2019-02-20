#include <chili/cont.h>

#if CBY_BACKEND_COUNT_ENABLED

#include "bytecode/opcodes.h"
#include "instrument/count.h"

CHI_INL void _countDestroy(CbyInterpreter* interp, countData_t* data) {
    cbyPrintInsnTable(interp, data, 0);
    countDestroy(interp, data);
}
#define countDestroy _countDestroy

#define INTERP_NAME count
#include "interp.h"

#endif
