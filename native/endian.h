#pragma once

#include "private.h"

// Convert little endian <-> host order
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
CHI_INL uint16_t chiLE16(uint16_t x) { return x; }
CHI_INL uint32_t chiLE32(uint32_t x) { return x; }
CHI_INL uint64_t chiLE64(uint64_t x) { return x; }
#else
CHI_INL uint16_t chiLE16(uint16_t x) { return __builtin_bswap16(x); }
CHI_INL uint32_t chiLE32(uint32_t x) { return __builtin_bswap32(x); }
CHI_INL uint64_t chiLE64(uint64_t x) { return __builtin_bswap64(x); }
#endif
