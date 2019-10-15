#pragma once

#define INSN_BEGIN(op)                                                  \
    Opcode _insnOp = op;                                                \
    chiStackCheckCanary(SL);                                            \
    instrumentInsnBegin(proc, &interpPS->data, IP - 2, _insnOp)

#define INSN_END instrumentInsnEnd(proc, &interpPS->data, _insnOp)

#define INSN_LEAVE                                              \
    ({                                                          \
        INSN_END;                                               \
        instrumentLeave(proc, &interpPS->data, CURRFN, SP);     \
    })                                                          \

#define FFIDESC(ref)                                                    \
    ({                                                                  \
        const CbyInterpModule* _mod = chiToCbyInterpModule(chiToCbyFn(CURRFN)->module); \
        const CbyCode* _oldip = IP;                                     \
        IP = (ref);                                                     \
        const uint32_t _id = FETCH32;                                   \
        CbyFFI* _ffi = chiToCbyFFI(_mod->ffi[_id]);                     \
        if (CHI_UNLIKELY(!_ffi->fn)) {                                  \
            INSN_LEAVE;                                                 \
            A(0) = chiFromIP(IP);                                       \
            KNOWN_JUMP(cbyFFIError);                                    \
        }                                                               \
        IP = _oldip;                                                    \
        _ffi;                                                           \
    })

#define DECLARE_CONTEXT                         \
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR; \
    CbyInterpPS* interpPS = proc->interpPS;     \
    Chili CURRFN;                               \
    const CbyCode* IP

#define RESTORE_CONTEXT(var)                    \
    ({                                          \
        CURRFN = SP[-4];                        \
        IP = chiToIP(SP[-3]);                   \
        SP[0] = (var);                          \
    })

#define SAVE_CONTEXT(sp, ip)                                            \
    ({                                                                  \
        Chili* frame = (sp) + CBY_CONTEXT_SIZE;                         \
        frame[-4] = CURRFN;                                             \
        frame[-3] = chiFromIP(ip);                                      \
        frame[-2] = chiFromUnboxed((ChiWord)(frame - SP));              \
        frame[-1] = chiFromCont(&interpCont);                           \
        CHI_ASSERT(frame > SP);                                         \
        SP = frame;                                                     \
        chiStackDebugWalk(proc->thread, SP, SL);                        \
    })
