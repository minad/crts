// Switch-threaded dispatch
#define OP_BEGIN(op)      case OP_##op: {
#define OP_END            INSN_END; goto _dispatch; }
#define DISPATCH_PROLOGUE uint16_t _opCode = 0
#define DISPATCH_BEGIN    _dispatch: _opCode = FETCH16; INSN_BEGIN((Opcode)_opCode); switch (_opCode) {
#define DISPATCH_END      default: CHI_BUG("Invalid instruction"); break; }
#define DISPATCH_INIT
#define DISPATCH_REWRITE  0
