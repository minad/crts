#include "cby.h"
#include "ffisupport.h"
#include "bytecode/opcodes.h"

#define _CBY_OPCODE_STRING(op) CHI_STRINGIZE(op),
const char* const cbyOpName[] = { CBY_FOREACH_OPCODE(_CBY_OPCODE_STRING) };
#undef _CBY_OPCODE_STRING

#define _CBY_FFI_TYPE_NAME(name, libffi, dcarg, dccall, type) #type,
const char* const cbyFFIType[] = { CHI_FOREACH_FFI_TYPE(, _CBY_FFI_TYPE_NAME) };
#undef _CBY_FFI_TYPE_NAME
