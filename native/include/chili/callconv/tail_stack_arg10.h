struct _ChiRegStore;
#define CHI_CONT_PROTO(name)                                          \
    __tailcallable__ name(struct _ChiRegStore* restrict _chiReg, \
                          Chili* restrict _chiRegSP, Chili* restrict _chiRegSL, \
                          ChiWord* restrict _chiRegHP, \
                          Chili _chiRegA0, Chili _chiRegA1, \
                          Chili _chiRegA2, Chili _chiRegA3, \
                          Chili _chiRegA4, Chili _chiRegA5)

#ifdef CONT

struct _ChiRegStore {
    ChiWord* hl;              ///< Heap limit
    Chili    a[CHI_AMAX - 6]; ///< Arguments
    ChiAuxRegs aux;
};

#define JUMP(c)       __tailcall__((ChiContFn*)(c), _chiReg, _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2, _chiRegA3, _chiRegA4, _chiRegA5)
#define FIRST_JUMP(n) ({ CHI_CONT_FN(n)(_chiReg, _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1, _chiRegA2, _chiRegA3, _chiRegA4, _chiRegA5); CHI_UNREACHABLE; })

#define SP   _chiRegSP
#define HP   _chiRegHP
#define SLRW _chiRegSL
#define A(i) (*({ size_t _i = (i); CHI_ASSERT(_i < CHI_AMAX);           \
                _i == 0 ? &_chiRegA0                                    \
                    : _i == 1 ? &_chiRegA1                              \
                    : _i == 2 ? &_chiRegA2                              \
                    : _i == 3 ? &_chiRegA3                              \
                    : _i == 4 ? &_chiRegA4                              \
                    : _i == 5 ? &_chiRegA5                              \
                    : _chiReg->a + _i - 6; }))

#define _PROLOGUE(na)                           \
    CHI_NOWARN_UNUSED(_chiReg);                 \
    CHI_NOWARN_UNUSED(_chiRegSP);               \
    CHI_NOWARN_UNUSED(_chiRegSL);               \
    CHI_NOWARN_UNUSED(_chiRegHP);               \
    CHI_NOWARN_UNUSED(_chiRegA0);               \
    if ((na) < 1) CHI_UNDEF(_chiRegA1);         \
    if ((na) < 2) CHI_UNDEF(_chiRegA2);         \
    if ((na) < 3) CHI_UNDEF(_chiRegA3);         \
    if ((na) < 4) CHI_UNDEF(_chiRegA4);         \
    if ((na) < 5) CHI_UNDEF(_chiRegA5)

#define CALLCONV_INIT                                                   \
    struct _ChiRegStore _chiRegStore = {}, *_chiReg = &_chiRegStore;   \
    Chili *_chiRegSP = 0, *_chiRegSL = 0;                               \
    ChiWord* _chiRegHP = 0;                                             \
    Chili _chiRegA0 = CHI_FALSE, _chiRegA1 = CHI_FALSE,                 \
        _chiRegA2 = CHI_FALSE, _chiRegA3 = CHI_FALSE,                   \
        _chiRegA4 = CHI_FALSE, _chiRegA5 = CHI_FALSE

#define CALLCONV_REGSTORE

#endif
