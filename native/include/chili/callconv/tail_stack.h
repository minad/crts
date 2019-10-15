struct _ChiRegStore;
#define CHI_CONT_PROTO(name) __tailcallable__ name(struct _ChiRegStore* _chiReg)

#ifdef CONT

struct _ChiRegStore {
    Chili*   sp;          ///< Stack pointer
    Chili*   sl;          ///< Stack limit
    ChiWord* hp;          ///< Heap pointer
    ChiWord* hl;          ///< Heap limit
    Chili    a[CHI_AMAX]; ///< Arguments
    uint8_t  na;          ///< Argument number
    ChiAuxRegs  aux;
};

#define JUMP(c)       __tailcall__((ChiContFn*)(c), _chiReg)
#define FIRST_JUMP(n) ({ CHI_CONT_FN(n)(_chiReg); CHI_UNREACHABLE; })

#define SP   (_chiReg->sp)
#define HP   (_chiReg->hp)
#define SLRW (_chiReg->sl)
#define NARW (_chiReg->na)
#define A(i) (*({ size_t _i = (i); CHI_ASSERT(_i < CHI_AMAX); _chiReg->a + _i; }))

#define _PROLOGUE(na)     CHI_NOWARN_UNUSED(_chiReg)
#define CALLCONV_INIT     struct _ChiRegStore _chiRegStore = {}, *_chiReg = &_chiRegStore
#define CALLCONV_REGSTORE

#endif
