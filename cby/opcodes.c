#include "bytecode/opcodes.h"
#include "cby.h"

#define _CBY_OPCODE_STRING(op) CHI_STRINGIZE(op),
CHI_INTERN const char* const cbyOpName[] = { CBY_FOREACH_OPCODE(_CBY_OPCODE_STRING) };
#undef _CBY_OPCODE_STRING
