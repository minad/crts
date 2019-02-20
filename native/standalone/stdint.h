#pragma once

typedef __UINT8_TYPE__   uint8_t;
typedef __UINT16_TYPE__	 uint16_t;
typedef __UINT32_TYPE__  uint32_t;
typedef __UINT64_TYPE__  uint64_t;
typedef __INT8_TYPE__    int8_t;
typedef __INT16_TYPE__   int16_t;
typedef __INT32_TYPE__   int32_t;
typedef __INT64_TYPE__   int64_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __INTPTR_TYPE__  intptr_t;

#define UINT8_MAX  __UINT8_MAX__
#define UINT16_MAX __UINT16_MAX__
#define UINT32_MAX __UINT32_MAX__
#define UINT64_MAX __UINT64_MAX__
#define INT8_MAX   __INT8_MAX__
#define INT16_MAX  __INT16_MAX__
#define INT32_MAX  __INT32_MAX__
#define INT64_MAX  __INT64_MAX__
#define INT8_MIN   (-INT8_MAX-1)
#define INT16_MIN  (-INT16_MAX-1)
#define INT32_MIN  (-INT32_MAX-1)
#define INT64_MIN  (-INT64_MAX-1)

#define INT8_C(x)   x
#define INT16_C(x)  x
#define INT32_C(x)  x
#define UINT8_C(x)  x
#define UINT16_C(x) x
#define UINT32_C(x) x

#if __SIZEOF_LONG__ == 8
#  define INT64_C(c)  (c ## L)
#  define UINT64_C(c) (c ## UL)
#else
#  define INT64_C(c)  (c ## LL)
#  define UINT64_C(c) (c ## ULL)
#endif
