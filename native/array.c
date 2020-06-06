#include "barrier.h"
#include "new.h"

CHI_INL void arrayWrite(ChiProcessor* proc, Chili array, uint32_t idx, Chili val) {
    chiFieldWrite(proc, array, chiArrayField(array, idx), val,
                  CHI_BARRIER_ALWAYS_MAJOR | (chiType(array) == CHI_ARRAY_LARGE ? CHI_BARRIER_CARD : 0));
}

CHI_INL bool arrayCas(ChiProcessor* proc, Chili array, uint32_t idx, Chili expected, Chili desired) {
    return chiFieldCas(proc, array, chiArrayField(array, idx), expected, desired,
                       CHI_BARRIER_ALWAYS_MAJOR | (chiType(array) == CHI_ARRAY_LARGE ? CHI_BARRIER_CARD : 0));
}

Chili chiArrayNewUninitialized(ChiProcessor* proc, uint32_t size, ChiNewFlags flags) {
    if (!size)
        return chiNewEmpty(CHI_ARRAY_SMALL);
    ChiType type = proc->rt->option.heap.noCard || size <= CHI_CARD_SIZE || !(flags & CHI_NEW_LOCAL)
                   ? CHI_ARRAY_SMALL : CHI_ARRAY_LARGE;
    return chiNewInl(proc, type, size, flags | (type == CHI_ARRAY_LARGE ? CHI_NEW_CARDS : 0));
}

Chili chiArrayNewFlags(uint32_t size, Chili val, ChiNewFlags flags) {
    // clean is only allowed if allocating uninitialized
    CHI_ASSERT(!(flags & CHI_NEW_UNINITIALIZED) == !(flags & CHI_NEW_CLEAN));
    if (!size)
        return chiNewEmpty(CHI_ARRAY_SMALL);
    // Allocate clean if val is unboxed or living in the major heap
    if (!(flags & CHI_NEW_UNINITIALIZED) && (!chiRef(val) || chiGen(val) == CHI_GEN_MAJOR))
        flags |= CHI_NEW_CLEAN;
    Chili c = chiArrayNewUninitialized(CHI_CURRENT_PROCESSOR, size, flags);
    if (!(flags & CHI_NEW_UNINITIALIZED) && chiSuccess(c)) {
        for (uint32_t i = 0; i < size; ++i)
            chiArrayInit(c, i, val);
    }
    return c;
}

Chili chiArrayTryClone(Chili src, uint32_t start, uint32_t size) {
    if (!size)
        return chiNewEmpty(CHI_ARRAY_SMALL);
    Chili c = chiArrayNewUninitialized(CHI_CURRENT_PROCESSOR, size, CHI_NEW_TRY | CHI_NEW_LOCAL);
    if (chiSuccess(c)) {
        for (uint32_t i = 0; i < size; ++i)
            chiArrayInit(c, i, chiFieldRead(chiArrayField(src, start + i)));
    }
    return c;
}

void chiArrayCopy(Chili src, uint32_t srcIdx, Chili dst, uint32_t dstIdx, uint32_t size) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (!chiIdentical(src, dst) || dstIdx < srcIdx) {
        for (uint32_t i = 0; i < size; ++i)
            arrayWrite(proc, dst, i, chiFieldRead(chiArrayField(src, srcIdx + i)));
    } else {
        for (uint32_t i = size; i --> 0;)
            arrayWrite(proc, dst, i, chiFieldRead(chiArrayField(src, srcIdx + i)));
    }
}

void chiArrayWrite(Chili array, uint32_t idx, Chili val) {
    arrayWrite(CHI_CURRENT_PROCESSOR, array, idx, val);
}

bool chiArrayCas(Chili array, uint32_t idx, Chili expected, Chili desired) {
    return arrayCas(CHI_CURRENT_PROCESSOR, array, idx, expected, desired);
}
