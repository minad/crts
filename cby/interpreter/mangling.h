/* Mangle interpreter names for easier analysis of the binary
 * and Redirect instrumentation functions.
 */
#ifndef INSTRUMENT_NAME
#  define INSTRUMENT_NAME INTERP_NAME
#endif
#ifndef INTERP_DISPATCH
#  define INTERP_DISPATCH CBY_DISPATCH
#endif
#define InstrumentData      CHI_CAT(INSTRUMENT_NAME, Data)
#define instrumentAlloc     CHI_CAT(INSTRUMENT_NAME, Alloc)
#define instrumentDestroy   CHI_CAT(INSTRUMENT_NAME, Destroy)
#define instrumentEnter     CHI_CAT(INSTRUMENT_NAME, Enter)
#define instrumentFFITail   CHI_CAT(INSTRUMENT_NAME, FFITail)
#define instrumentFFIBegin  CHI_CAT(INSTRUMENT_NAME, FFIBegin)
#define instrumentFFIEnd    CHI_CAT(INSTRUMENT_NAME, FFIEnd)
#define instrumentInsnBegin CHI_CAT(INSTRUMENT_NAME, InsnBegin)
#define instrumentInsnEnd   CHI_CAT(INSTRUMENT_NAME, InsnEnd)
#define instrumentLeave     CHI_CAT(INSTRUMENT_NAME, Leave)
#define instrumentProcStart CHI_CAT(INSTRUMENT_NAME, ProcStart)
#define instrumentProcStop  CHI_CAT(INSTRUMENT_NAME, ProcStop)
#define instrumentSetup     CHI_CAT(INSTRUMENT_NAME, Setup)
#define interpBackend       CHI_CAT(INTERP_NAME, _interpBackend)
#define interpCont          CHI_CAT(INTERP_NAME, _interpCont)
#define interpFn            CHI_CAT(INTERP_NAME, _interpFn)
#define interpProcStart     CHI_CAT(INTERP_NAME, _interpProcStart)
#define interpProcStop      CHI_CAT(INTERP_NAME, _interpProcStop)
#define interpSetup         CHI_CAT(INTERP_NAME, _interpSetup)
#define interpreter         CHI_CAT(INTERP_NAME, _interpreter)
#include CHI_STRINGIZE(../dispatch/INTERP_DISPATCH.h)
