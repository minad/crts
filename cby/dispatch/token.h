// Token-threaded dispatch (goto with lookup table)
#define _DISPATCH_JUMP    goto *tokenDispatch[FETCH16]
#define OP_BEGIN(op)      LABEL_##op: { INSN_BEGIN(OP_##op); const CbyCode* _insnBegin = IP - 2; CHI_NOWARN_UNUSED(_insnBegin);
#define OP_END            INSN_END; _DISPATCH_JUMP; }
#define _CBY_OPCODE_LABEL(op, name) &&LABEL_##op,
#define DISPATCH_PROLOGUE \
    static const void* const tokenDispatch[] = { CBY_FOREACH_OPCODE(_CBY_OPCODE_LABEL) }
#define DISPATCH_BEGIN    _DISPATCH_JUMP;
#define DISPATCH_END
#define DISPATCH_INIT
#define DISPATCH_REWRITE  0
