#pragma once

#include "private.h"

CHI_INTERN size_t chiUtf8Size(const uint16_t*);
CHI_INTERN size_t chiUtf16Size(const char*);
CHI_INTERN void chiUtf16To8(const uint16_t*, char*, size_t);
CHI_INTERN void chiUtf8To16(const char*, uint16_t*, size_t);
CHI_INTERN char* chiAllocUtf16To8(const uint16_t*);
CHI_INTERN uint16_t* chiAllocUtf8To16(const char*);
