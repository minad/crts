struct _ChiRegStore;
#define CHI_CONT_PROTO(name) CHI_WU void* name(struct _ChiRegStore* _chiReg)

#ifdef CONT

struct _ChiRegStore {
    Chili*   sp;          ///< Stack pointer
    Chili*   sl;          ///< Stack limit
    ChiWord* hp;          ///< Heap pointer
    ChiWord* hl;          ///< Heap limit
    Chili    a[CHI_AMAX]; ///< Arguments
    ChiAuxRegs  aux;
};

#define SP    (_chiReg->sp)
#define HP    (_chiReg->hp)
#define SLRW  (_chiReg->sl)
#define _A(i) (_chiReg-> a + (i))

#define JUMP(c)       ({ ChiContFn* _c = (ChiContFn*)(c); return _c; })
#define FIRST_JUMP(n) ({ ChiContFn* _c = CHI_CONT_FN(n); for (;;) _c = (ChiContFn*)(*_c)(_chiReg); })

#define _PROLOGUE         CHI_NOWARN_UNUSED(_chiReg)
#define _UNDEF_ARGS(n)    ({})
#define CALLCONV_INIT     struct _ChiRegStore _chiRegStore = {}, *_chiReg = &_chiRegStore
#define CALLCONV_REGSTORE

#endif
