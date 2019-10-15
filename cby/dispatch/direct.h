// Direct-threaded dispatch
#define startTable       CHI_CAT(__start_cby_direct_dispatch_, INTERP_NAME)
#define stopTable        CHI_CAT(__stop_cby_direct_dispatch_,  INTERP_NAME)
#define interpreterFn    CHI_CONT_FN(CHI_CAT(INTERP_NAME, _interpreter))
#define DISPATCH_REWRITE CHI_CAT(INTERP_NAME, _rewrite)

extern void *startTable[], *stopTable[];

static void DISPATCH_REWRITE(CbyCode* ip, const CbyCode* end) {
    cbyDirectDispatchRewrite(startTable, ip, end);
}

#define _DISPATCH_JUMP   goto *(startAddress + ((uint32_t)FETCH16 << CBY_DIRECT_DISPATCH_SHIFT))
#define OP_BEGIN(op)     LABEL_##op: { INSN_BEGIN(OP_##op);
#define OP_END           INSN_END; _DISPATCH_JUMP; }
#define _CBY_OPCODE_LABEL(op) &&LABEL_##op,
#define DISPATCH_PROLOGUE                                               \
    static __attribute__((section("cby_direct_dispatch_" CHI_STRINGIZE(INTERP_NAME)), used)) \
    void* CHI_CAT(direct_dispatch_, INTERP_NAME)[]                      \
    = { CBY_FOREACH_OPCODE(_CBY_OPCODE_LABEL) };                        \
    const uint8_t* startAddress = (const uint8_t*)(const void*)interpreterFn
#define DISPATCH_BEGIN    _DISPATCH_JUMP;
#define DISPATCH_END
#define DISPATCH_INIT     CHI_CONSTRUCTOR(CHI_CAT(cby_direct_dispatch, INTERP_NAME)) { cbyDirectDispatchInit(CHI_STRINGIZE(INTERP_NAME), startTable, stopTable, interpreterFn); }
