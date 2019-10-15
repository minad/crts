#include "barrier.h"
#include "new.h"

Chili chiArrayNewUninitialized(ChiProcessor* proc, uint32_t size, ChiNewFlags flags) {
    if (!size)
        return chiNewEmpty(CHI_ARRAY);
    return chiNewInl(proc, CHI_ARRAY, size, flags | CHI_NEW_LOCAL);
}

Chili chiArrayNewFlags(uint32_t size, Chili val, ChiNewFlags flags) {
    if (!size)
        return chiNewEmpty(CHI_ARRAY);
    Chili c = chiArrayNewUninitialized(CHI_CURRENT_PROCESSOR, size, flags);
    if (chiSuccess(c) && !(flags & CHI_NEW_UNINITIALIZED)) {
        for (uint32_t i = 0; i < size; ++i)
            chiArrayInit(c, i, val);
    }
    return c;
}

Chili chiArrayTryClone(Chili src, uint32_t start, uint32_t size) {
    if (!size)
        return chiNewEmpty(CHI_ARRAY);
    Chili c = chiArrayNewUninitialized(CHI_CURRENT_PROCESSOR, size, CHI_NEW_TRY);
    if (chiSuccess(c)) {
        for (uint32_t i = 0; i < size; ++i)
            chiArrayInit(c, i, chiFieldRead(chiArrayField(src, start + i)));
    }
    return c;
}

// TODO: Use an optimized write barrier!
void chiArrayCopy(Chili src, uint32_t srcIdx, Chili dst, uint32_t dstIdx, uint32_t size) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    if (!chiIdentical(src, dst) || dstIdx < srcIdx) {
        for (uint32_t i = 0; i < size; ++i)
            chiFieldWriteMajor(proc, dst, chiArrayField(dst, dstIdx + i),
                               chiFieldRead(chiArrayField(src, srcIdx + i)));
    } else {
        for (uint32_t i = size; i --> 0;)
            chiFieldWriteMajor(proc, dst, chiArrayField(dst, dstIdx + i),
                               chiFieldRead(chiArrayField(src, srcIdx + i)));
    }
}

void chiArrayWrite(Chili array, uint32_t idx, Chili val) {
    chiFieldWriteMajor(CHI_CURRENT_PROCESSOR, array, chiArrayField(array, idx), val);
}

bool chiArrayCas(Chili array, uint32_t idx, Chili expected, Chili desired) {
    return chiFieldCasMajor(CHI_CURRENT_PROCESSOR, array, chiArrayField(array, idx), expected, desired);
}
