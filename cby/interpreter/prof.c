#include <chili/cont.h>

#if CBY_BACKEND_PROF_ENABLED

#include "../bytecode/opcodes.h"
#include "../instrument/prof.h"
#include "native/sink.h"

#define INTERP_NAME prof
#include "interpreter.h"

#endif
