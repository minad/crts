struct _ChiRegStore_;
#define CHI_CONT_FN(name) CHI_WU void* name(struct _ChiRegStore_* _chiReg)

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

#define AUX  (_chiReg->aux)
#define SP   (_chiReg->sp)
#define HP   (_chiReg->hp)
#define A(i) (*({ size_t _i = (i); CHI_ASSERT(_i < CHI_AMAX); _chiReg-> a + _i; }))
#define SLRW (_chiReg->sl)
#define HLRW (_chiReg->hl)
#define NARW (_chiReg->na)

#define _CALLCONV_JUMP(c)       ({ ChiContFn* _c = (ChiContFn*)(c); return _c; })
#define _CALLCONV_FIRST_JUMP(c) ({ ChiContFn* _c = (ChiContFn*)(c); for (;;) _c = (ChiContFn*)(*_c)(_chiReg); })

#define _PROLOGUE         CHI_NOWARN_UNUSED(_chiReg)
#define CALLCONV_INIT     struct _ChiRegStore_ _chiRegStore = {}, *_chiReg = &_chiRegStore
#define CALLCONV_REGSTORE

#endif
