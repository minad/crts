#pragma once

#define INSN_BEGIN(op)                                                  \
    Opcode _insnOp = op;                                                \
    chiStackCheckCanary(SL);                                            \
    instrumentInsnBegin(proc, &interpPS->data, IP - 2, _insnOp)

#define ENTER(jmp) instrumentEnter(proc, &interpPS->data, CURRFN, SP, jmp)
#define LEAVE      instrumentLeave(proc, &interpPS->data, CURRFN, SP)
#define INSN_END   instrumentInsnEnd(proc, &interpPS->data, _insnOp)
#define INSN_LEAVE ({ INSN_END; LEAVE; })

#define FFIDESC(ref)                                                    \
    ({                                                                  \
        const CbyInterpModule* _mod = chiToCbyInterpModule(chiToCbyFn(CURRFN)->module); \
        const CbyCode* _oldip = IP;                                     \
        IP = (ref);                                                     \
        const uint32_t _id = FETCH32;                                   \
        CbyFFI* _ffi = chiToCbyFFI(_mod->ffi[_id]);                     \
        if (CHI_UNLIKELY(!_ffi->fn)) {                                  \
            INSN_LEAVE;                                                 \
            ASET(0, chiFromIP(IP));                                     \
            KNOWN_JUMP(cbyFFIError);                                    \
        }                                                               \
        IP = _oldip;                                                    \
        _ffi;                                                           \
    })

#define FETCH_ENTER                             \
    CURRFN = SP[-4];                            \
    IP = chiToIP(SP[-3]);                       \
    FETCH16; /* OP_enter */                     \
    uint8_t nargs = FETCH8;                     \
    Chili* top = SP - CBY_CONTEXT_SIZE;         \
    CHI_NOWARN_UNUSED(nargs);                   \
    REG = SP -= chiToUnboxed(SP[-2])

#define SAVE_CONT(ip)                                   \
    ({                                                  \
        SP = top + CBY_CONTEXT_SIZE;                    \
        top[0] = CURRFN;                                \
        top[1] = chiFromIP(ip);                         \
        top[2] = chiFromUnboxed((ChiWord)(SP - REG));   \
        top[3] = chiFromCont(&interpCont);              \
    })
