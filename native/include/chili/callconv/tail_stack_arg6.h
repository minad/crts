struct _ChiRegStore;
#define CHI_CONT_PROTO(name)                                          \
    __tailcallable__ name(struct _ChiRegStore* restrict _chiReg, \
                          Chili* restrict _chiRegSP, Chili* restrict _chiRegSL, \
                          ChiWord* restrict _chiRegHP, \
                          Chili _chiRegA0, Chili _chiRegA1)

#ifdef CONT

struct _ChiRegStore {
    ChiWord* hl;              ///< Heap limit
    Chili    a[CHI_AMAX - 2]; ///< Arguments
    ChiAuxRegs  aux;
};

#define JUMP(c)       __tailcall__((ChiContFn*)(c), _chiReg, _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1)
#define FIRST_JUMP(n) ({ CHI_CONT_FN(n)(_chiReg, _chiRegSP, _chiRegSL, _chiRegHP, _chiRegA0, _chiRegA1); CHI_UNREACHABLE; })

#define SP   _chiRegSP
#define HP   _chiRegHP
#define SLRW _chiRegSL
#define A(i) (*({ size_t _i = (i); CHI_ASSERT(_i < CHI_AMAX); _i == 0 ? &_chiRegA0 : _i == 1 ? &_chiRegA1 : _chiReg->a + _i - 2; }))

#define _PROLOGUE(na)                           \
    CHI_NOWARN_UNUSED(_chiReg);                 \
    CHI_NOWARN_UNUSED(_chiRegSP);               \
    CHI_NOWARN_UNUSED(_chiRegSL);               \
    CHI_NOWARN_UNUSED(_chiRegHP);               \
    CHI_NOWARN_UNUSED(_chiRegA0);               \
    if ((na) < 1) CHI_UNDEF(_chiRegA1)

#define CALLCONV_INIT                                                   \
    struct _ChiRegStore _chiRegStore = {}, *_chiReg = &_chiRegStore;   \
    Chili *_chiRegSP = 0, *_chiRegSL = 0;                               \
    ChiWord* _chiRegHP = 0;                                             \
    Chili _chiRegA0 = CHI_FALSE, _chiRegA1 = CHI_FALSE

#define CALLCONV_REGSTORE

#endif
