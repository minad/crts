#include <chili/cont.h>

#if CBY_BACKEND_DUMP_ENABLED

#include "../bytecode/opcodes.h"
#include "../instrument/dump.h"

// we cannot use direct dispatch, since the dumper cannot handle decoded opcodes
// switch dispatch generates the shortest code!
#define INTERP_DISPATCH switch
#define INTERP_NAME     dump
#include "interpreter.h"

#endif
