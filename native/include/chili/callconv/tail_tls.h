#include "tailcall.h"

#define CHI_CONT_FN(name) __tailcallable__ name(void)

#ifdef CONT

struct _ChiRegStore_ {
    Chili*   sp;          ///< Stack pointer
    Chili*   sl;          ///< Stack limit
    ChiWord* hp;          ///< Heap pointer
    ChiWord* hl;          ///< Heap limit
    Chili    a[CHI_AMAX]; ///< Arguments
    uint8_t  na;          ///< Argument number
    _ChiAux  aux;
};

#define _CALLCONV_JUMP(c)       __tailcall__((ChiContFn*)(c))
#define _CALLCONV_FIRST_JUMP(c) ({ ((ChiContFn*)(c))(); CHI_UNREACHABLE; })

#define AUX  (_chiReg->aux)
#define SP   (_chiReg->sp)
#define HP   (_chiReg->hp)
#define A(i) (*({ size_t _i = (i); CHI_ASSERT(_i < CHI_AMAX); _chiReg->a + _i; }))
#define SLRW (_chiReg->sl)
#define HLRW (_chiReg->hl)
#define NARW (_chiReg->na)

#define _PROLOGUE         struct _ChiRegStore_* _chiReg = &_chiRegStore
#define CALLCONV_INIT     _PROLOGUE
#define CALLCONV_REGSTORE CHI_API _Thread_local struct _ChiRegStore_ _chiRegStore;

extern CALLCONV_REGSTORE

#endif
