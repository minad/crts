#pragma once

#include "event/defs.h"
#include "private.h"

enum {
    CHI_STACK_CONT_SIZE = 1,
};

CHI_OBJECT(STACK, Stack, Chili* sp; uint32_t _pad[CHI_ARCH_BITS == 32]; Chili base[])

typedef struct ChiProcessor_ ChiProcessor;
typedef struct ChiSink_ ChiSink;

CHI_WU Chili chiStackNew(ChiProcessor*);
CHI_WU Chili chiStackTryResize(ChiProcessor*, Chili*, Chili*);
CHI_WU Chili* chiStackUnwind(ChiProcessor*, Chili);
CHI_WU Chili chiStackGetTrace(ChiProcessor*, Chili*);
void chiStackDump(ChiSink*, ChiProcessor*);
void chiStackBlackhole(ChiProcessor*);
void chiStackActivate(ChiProcessor*);
void chiStackDeactivate(ChiProcessor*, Chili*);
void chiStackDeactivateReset(ChiProcessor*);

CHI_INL Chili* chiStackLimit(Chili c) {
    CHI_ASSERT(chiType(c) == CHI_STACK);
    return chiPayload(c) + chiSize(c) - (CHI_STACK_CONT_SIZE + CHI_STACK_CANARY_SIZE);
}

CHI_INL void chiStackCheckCanary(Chili* sl) {
    if (CHI_STACK_CANARY_SIZE) {
        sl += CHI_STACK_CONT_SIZE;
        for (size_t i = 0; i < CHI_MAX(1U, CHI_STACK_CANARY_SIZE); ++i, ++sl)
            CHI_ASSERT(chiIdentical(*sl, (Chili){ CHI_STACK_CANARY }));
    }
}

CHI_INL void chiStackInitCanary(Chili c) {
    if (CHI_STACK_CANARY_SIZE) {
        Chili* sl = chiStackLimit(c) + CHI_STACK_CONT_SIZE;
        for (size_t i = 0; i < CHI_MAX(1U, CHI_STACK_CANARY_SIZE); ++i)
            *sl++ = (Chili){ CHI_STACK_CANARY };
    }
}

CHI_INL CHI_WU const ChiContInfo* chiContInfo(ChiCont cont) {
#if CHI_CONTINFO_PREFIX
    void* linkcheck = (uint8_t*)cont - 2 * CHI_CONTINFO_OFFSET;
    CHI_ASSERT(linkcheck == *(void**)linkcheck);
    return CHI_ALIGN_CAST((uint8_t*)cont - CHI_CONTINFO_OFFSET, const ChiContInfo*);
#else
    return cont;
#endif
}

CHI_INL CHI_WU const ChiContInfo* chiFrameInfo(const Chili* p) {
    return chiContInfo(chiToCont(*p));
}

CHI_INL CHI_WU ChiFrame chiFrame(const Chili* p) {
    return chiFrameInfo(p)->type;
}

CHI_INL CHI_WU size_t chiFrameSize(const Chili* p) {
    size_t size = chiFrameInfo(p)->size;
    if (size == CHI_VASIZE)
        size = (size_t)chiToUnboxed(p[-1]);
    CHI_ASSERT(size >= 1);
    return size;
}

CHI_INL CHI_WU bool chiFrameIdentical(const Chili* p, const Chili* q) {
    if (!p || !q)
        return false;
    if (chiFrame(p) == CHI_FRAME_INTERP && chiFrame(p) == chiFrame(q)) {
        p = p - 3;
        q = q - 3;
    }
    return chiIdentical(*p, *q);
}

CHI_INL CHI_WU uint32_t chiFrameArgs(const Chili* p, Chili regA0, uint8_t regNA) {
    const ChiContInfo* info = chiFrameInfo(p);
    uint8_t na = info->na;
    if (na == CHI_VAARGS) {
        if (info->type == CHI_FRAME_APPN)
            return regNA;
        return chiFnOrThunkArity(regA0);
    }
    return na;
}

CHI_INL void chiStackDebugWalk(const Chili* base, const Chili* sp) {
    if (CHI_STACK_DEBUG_WALK) {
        const Chili* p = sp - 1;
        for (; p >= base; p -= chiFrameSize(p));
        CHI_ASSERT(p == base - 1);
    }
}
