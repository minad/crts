#pragma once

#include "private.h"

size_t chiUtf8Size(const uint16_t*);
size_t chiUtf16Size(const char*);
void chiUtf16To8(const uint16_t*, char*, size_t);
void chiUtf8To16(const char*, uint16_t*, size_t);
char* chiAllocUtf16To8(const uint16_t*);
uint16_t* chiAllocUtf8To16(const char*);
