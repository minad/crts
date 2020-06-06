#define CHI_CONT_PROTO(name)                                          \
    __tailcallable__ name(Chili* restrict _chiRegSP, Chili* restrict _chiRegSL, \
                          ChiWord* restrict _chiRegHP, \
                          Chili _chiRegA0, Chili _chiRegA1, \
                          Chili _chiRegA2, Chili _chiRegA3, \
                          Chili _chiRegA4, Chili _chiRegA5, \
                          Chili _chiRegA6)

#ifdef CONT

struct _ChiRegStore {
    ChiWord* hl;              ///< Heap limit
    Chili    a[CHI_AMAX - 7]; ///< Arguments
    ChiAuxRegs aux;
};

#define JUMP(c)       __tailcall__((ChiContFn*)(c), _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2, _chiRegA3, _chiRegA4, _chiRegA5, _chiRegA6)
#define FIRST_JUMP(n) ({ CHI_CONT_FN(n)(_chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2, _chiRegA3, _chiRegA4, _chiRegA5, _chiRegA6); CHI_UNREACHABLE; })

#define SP    _chiRegSP
#define HP    _chiRegHP
#define SLRW  _chiRegSL
#define _A(i) ((i) == 0 ? &_chiRegA0             \
               : (i) == 1 ? &_chiRegA1           \
               : (i) == 2 ? &_chiRegA2           \
               : (i) == 3 ? &_chiRegA3           \
               : (i) == 4 ? &_chiRegA4           \
               : (i) == 5 ? &_chiRegA5           \
               : (i) == 6 ? &_chiRegA6           \
               : _chiReg->a + (i) - 7)

#define _PROLOGUE                                       \
    struct _ChiRegStore* _chiReg = &_chiRegStore;       \
    CHI_NOWARN_UNUSED(_chiReg);                         \
    CHI_NOWARN_UNUSED(_chiRegSP);                       \
    CHI_NOWARN_UNUSED(_chiRegSL);                       \
    CHI_NOWARN_UNUSED(_chiRegHP);                       \
    CHI_NOWARN_UNUSED(_chiRegA0);                       \
    CHI_NOWARN_UNUSED(_chiRegA1);                       \
    CHI_NOWARN_UNUSED(_chiRegA2);                       \
    CHI_NOWARN_UNUSED(_chiRegA3);                       \
    CHI_NOWARN_UNUSED(_chiRegA4);                       \
    CHI_NOWARN_UNUSED(_chiRegA5);                       \
    CHI_NOWARN_UNUSED(_chiRegA6)

#define _UNDEF_ARGS(n)                          \
    ({                                          \
        if ((n) <= 0) CHI_UNDEF(_chiRegA0);     \
        if ((n) <= 1) CHI_UNDEF(_chiRegA1);     \
        if ((n) <= 2) CHI_UNDEF(_chiRegA2);     \
        if ((n) <= 3) CHI_UNDEF(_chiRegA3);     \
        if ((n) <= 4) CHI_UNDEF(_chiRegA4);     \
        if ((n) <= 5) CHI_UNDEF(_chiRegA5);     \
        if ((n) <= 6) CHI_UNDEF(_chiRegA6);     \
    })

#define CALLCONV_INIT                                                   \
    struct _ChiRegStore *_chiReg = &_chiRegStore;                      \
    Chili *_chiRegSP = 0, *_chiRegSL = 0;                               \
    ChiWord* _chiRegHP = 0;                                             \
    Chili _chiRegA0 = CHI_FALSE, _chiRegA1 = CHI_FALSE, _chiRegA2 = CHI_FALSE, \
        _chiRegA3 = CHI_FALSE, _chiRegA4 = CHI_FALSE,                   \
        _chiRegA5 = CHI_FALSE, _chiRegA6 = CHI_FALSE

#define CALLCONV_REGSTORE CHI_EXPORT _Thread_local struct _ChiRegStore _chiRegStore;
extern CALLCONV_REGSTORE

#endif
