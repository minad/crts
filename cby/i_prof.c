#include <chili/cont.h>
#include "cby.h"

#if CBY_BACKEND_PROF_ENABLED

#include "stack.h"
#include "sink.h"
#include "trace.h"
#include "bytecode/opcodes.h"
#include "instrument/prof.h"

#define INTERP_NAME prof
#include "interp.h"

#undef CHI_PROF_ENABLED
#define CHI_PROF_ENABLED 1
#define CHI_PROF_CBY_SUPPORT 1
#include "../native/profiler.c"

#endif
