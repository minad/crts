#pragma once

#include <stdarg.h>
#include "private.h"

CHI_INTERN _Noreturn void chiSysErr(const char*);
CHI_INTERN _Noreturn void chiErr(const char*, ...) CHI_FMT(1, 2);
CHI_INTERN void chiWarn(const char*, ...) CHI_FMT(1, 2);
