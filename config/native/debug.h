#include "release.h"

#undef CHI_INL
#define CHI_INL static __attribute__ ((unused))

#undef  CHI_CALLCONV
#ifdef CHI_STANDALONE_WASM
#  define CHI_CALLCONV            trampoline_tls
#else
#  define CHI_CALLCONV            trampoline_stack
#endif

#undef  CBY_DISPATCH
#define CBY_DISPATCH              switch

#undef  CHI_LIKELY_STATS
#define CHI_LIKELY_STATS          0

#undef  CHI_POISON_ENABLED
#define CHI_POISON_ENABLED        1

#undef  CHI_POISON_STACK_CANARY_SIZE
#define CHI_POISON_STACK_CANARY_SIZE 1

#undef  CHI_FNLOG_ENABLED
#define CHI_FNLOG_ENABLED         1
/*
#undef  CHI_POISON_OBJECT_ENABLED
#define CHI_POISON_OBJECT_ENABLED 1

#undef  CHI_POISON_BLOCK_ENABLED
#define CHI_POISON_BLOCK_ENABLED  1

#undef  CHI_STACK_DEBUG_WALK
#define CHI_STACK_DEBUG_WALK      1
*/
