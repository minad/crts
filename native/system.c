#include "system.h"
#include "../error.h"
#include "../sink.h"

#if CHI_SYSTEM_HAS_STDIO
#  include "system/stdio_quirks.h"
#endif

#if defined(CHI_STANDALONE_SANDBOX)
#  include "system/sandbox_impl.h"
#elif defined(CHI_STANDALONE_WASM)
#  include "system/wasm_impl.h"
#elif defined(_WIN32)
#  include "system/win_impl.h"
#else
#  include "system/posix_impl.h"
#endif
