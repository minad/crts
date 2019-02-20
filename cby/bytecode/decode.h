#pragma once

#include <chili/object/string.h>

#ifndef CHECK_RANGE
#  define CHECK_RANGE(n) ({})
#endif

#define FETCH8  ({ CHECK_RANGE(1); *IP++; })
#define FETCH16 ({ CHECK_RANGE(2); IP += 2; chiPeekUInt16(IP - 2); })
#define FETCH32 ({ CHECK_RANGE(4); IP += 4; chiPeekUInt32(IP - 4); })
#define FETCH64 ({ CHECK_RANGE(8); IP += 8; chiPeekUInt64(IP - 8); })
#define FETCH_STRING                                                    \
    ({                                                                  \
        const uint32_t _off = FETCH32;                                  \
        typeof(IP) _old = IP;                                           \
        IP -= _off;                                                     \
        const uint32_t _size = FETCH32;                                 \
        const CbyCode* _bytes = IP;                                     \
        CHECK_RANGE(_size);                                             \
        IP = _old;                                                      \
        (ChiStringRef){ .bytes = _bytes, .size = _size };               \
    })
