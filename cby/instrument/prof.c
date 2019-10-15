#include "prof.h"

#if CBY_BACKEND_PROF_ENABLED

#include "native/profiler.h"

_Static_assert(CHI_PROF_ENABLED, "Profiler must be enabled");

void _profSetup(CbyInterpreter* interp) {
    chiProfilerSetup(&interp->rt, &interp->option.prof);
}

#endif
