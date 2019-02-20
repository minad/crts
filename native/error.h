#pragma once

#include "private.h"

_Noreturn void chiAbort(const char*, ...) CHI_FMT(1, 2);
_Noreturn void chiSysErr(const char*);
_Noreturn void chiErr(const char*, ...) CHI_FMT(1, 2);
void chiWarn(const char*, ...) CHI_FMT(1, 2);
