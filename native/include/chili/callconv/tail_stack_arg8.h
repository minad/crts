#include "tailcall.h"

struct _ChiRegStore_;
#define CHI_CONT_FN(name)                                          \
    __tailcallable__ name(struct _ChiRegStore_* restrict _chiReg, \
                          Chili* restrict _chiRegSP, Chili* restrict _chiRegSL, \
                          ChiWord* restrict _chiRegHP, \
                          Chili _chiRegA0, Chili _chiRegA1, \
                          Chili _chiRegA2, Chili _chiRegA3)

#ifdef CONT

struct _ChiRegStore_ {
    ChiWord* hl;              ///< Heap limit
    Chili    a[CHI_AMAX - 4]; ///< Arguments
    uint8_t  na;              ///< Argument number
    _ChiAux  aux;
};

#define _CALLCONV_JUMP(c)       __tailcall__((ChiContFn*)(c), _chiReg, _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2, _chiRegA3)
#define _CALLCONV_FIRST_JUMP(c) ({ ((ChiContFn*)(c))(_chiReg, _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2, _chiRegA3); CHI_UNREACHABLE; })

#define AUX  (_chiReg->aux)
#define SP   _chiRegSP
#define HP   _chiRegHP
#define A(i) (*({ size_t _i = (i); CHI_ASSERT(_i < CHI_AMAX); _i == 0 ? &_chiRegA0 : _i == 1 ? &_chiRegA1 : _i == 2 ? &_chiRegA2 : _i == 3 ? &_chiRegA3 : _chiReg->a + _i - 4; }))
#define SLRW _chiRegSL
#define HLRW (_chiReg->hl)
#define NARW (_chiReg->na)

#define _PROLOGUE                                                 \
    CHI_NOWARN_UNUSED(_chiReg);                                   \
    CHI_NOWARN_UNUSED(_chiRegSP);                                 \
    CHI_NOWARN_UNUSED(_chiRegSL);                                 \
    CHI_NOWARN_UNUSED(_chiRegHP);                                 \
    CHI_NOWARN_UNUSED(_chiRegA0);                                 \
    CHI_NOWARN_UNUSED(_chiRegA1);                                 \
    CHI_NOWARN_UNUSED(_chiRegA2);                                 \
    CHI_NOWARN_UNUSED(_chiRegA3)

#define CALLCONV_INIT                                                   \
    struct _ChiRegStore_ _chiRegStore = {}, *_chiReg = &_chiRegStore; \
    Chili *_chiRegSP = 0, *_chiRegSL = 0;                           \
    ChiWord* _chiRegHP = 0;                                         \
    Chili _chiRegA0 = CHI_FALSE, _chiRegA1 = CHI_FALSE, _chiRegA2 = CHI_FALSE, _chiRegA3 = CHI_FALSE

#define CALLCONV_REGSTORE

#endif
