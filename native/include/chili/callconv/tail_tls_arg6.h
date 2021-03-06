#define CHI_CONT_PROTO(name)                                          \
    __tailcallable__ name(Chili* restrict _chiRegSP, Chili* restrict _chiRegSL, \
                          ChiWord* restrict _chiRegHP, \
                          Chili _chiRegA0, Chili _chiRegA1, \
                          Chili _chiRegA2)

#ifdef CONT

struct _ChiRegStore {
    ChiWord* hl;              ///< Heap limit
    Chili    a[CHI_AMAX - 3]; ///< Arguments
    ChiAuxRegs  aux;
};

#define JUMP(c)       __tailcall__((ChiContFn*)(c), _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2)
#define FIRST_JUMP(n) ({ CHI_CONT_FN(n)(_chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2); CHI_UNREACHABLE; })

#define SP   _chiRegSP
#define HP   _chiRegHP
#define SLRW _chiRegSL
#define _A(i) ((i) == 0 ? &_chiRegA0 : (i) == 1 ? &_chiRegA1 : (i) == 2 ? &_chiRegA2 : _chiReg->a + (i) - 3)

#define _PROLOGUE                                       \
    struct _ChiRegStore* _chiReg = &_chiRegStore;       \
    CHI_NOWARN_UNUSED(_chiReg);                         \
    CHI_NOWARN_UNUSED(_chiRegSP);                       \
    CHI_NOWARN_UNUSED(_chiRegSL);                       \
    CHI_NOWARN_UNUSED(_chiRegHP);                       \
    CHI_NOWARN_UNUSED(_chiRegA0);                       \
    CHI_NOWARN_UNUSED(_chiRegA1);                       \
    CHI_NOWARN_UNUSED(_chiRegA2)

#define _UNDEF_ARGS(n)                          \
    ({                                          \
        if ((n) <= 0) CHI_UNDEF(_chiRegA0);     \
        if ((n) <= 1) CHI_UNDEF(_chiRegA1);     \
        if ((n) <= 2) CHI_UNDEF(_chiRegA2);     \
    })

#define CALLCONV_INIT                                                   \
    struct _ChiRegStore *_chiReg = &_chiRegStore;                      \
    Chili *_chiRegSP = 0, *_chiRegSL = 0;                               \
    ChiWord* _chiRegHP = 0;                                             \
    Chili _chiRegA0 = CHI_FALSE, _chiRegA1 = CHI_FALSE, _chiRegA2 = CHI_FALSE

#define CALLCONV_REGSTORE CHI_EXPORT _Thread_local struct _ChiRegStore _chiRegStore;
extern CALLCONV_REGSTORE

#endif
