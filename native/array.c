#include <chili/object/array.h>
#include "barrier.h"
#include "processor.h"

Chili chiArrayNewFlags(uint32_t size, Chili val, uint32_t flags) {
    if (!size)
        return chiNewEmpty(CHI_ARRAY);
    Chili array = chiNewFlags(CHI_ARRAY, size, flags);
    if (chiSuccess(array) && !(flags & CHI_NEW_UNINITIALIZED)) {
        for (uint32_t i = 0; i < size; ++i)
            chiAtomicInit(chiArrayField(array, i), val);
    }
    return array;
}

Chili chiArrayTryNew(uint32_t size, Chili val) {
    return chiArrayNewFlags(size, val, CHI_NEW_TRY);
}

Chili chiArrayTryClone(Chili array, uint32_t start, uint32_t size) {
    CHI_ASSERT(chiType(array) == CHI_ARRAY);
    Chili result = chiArrayNewFlags(size, CHI_FALSE, CHI_NEW_TRY | CHI_NEW_UNINITIALIZED);
    if (chiSuccess(result)) {
        for (uint32_t i = 0; i < size; ++i)
            chiAtomicStore(chiArrayField(result, i), chiArrayRead(array, start + i));
    }
    return result;
}

void chiArrayCopy(Chili src, uint32_t srcIdx, Chili dst, uint32_t dstIdx, uint32_t size) {
    if (!size)
        return;
    if (!chiIdentical(src, dst) || dstIdx < srcIdx) {
        for (uint32_t i = 0; i < size; ++i)
            chiAtomicStore(chiArrayField(dst, dstIdx + i), chiArrayRead(src, srcIdx + i));
    } else {
        for (uint32_t i = size; i --> 0;)
            chiAtomicStore(chiArrayField(dst, dstIdx + i), chiArrayRead(src, srcIdx + i));
    }
    chiWriteBarrier(CHI_CURRENT_PROCESSOR, dst);
}

void chiArrayWrite(Chili array, uint32_t idx, Chili val) {
    chiWriteField(CHI_CURRENT_PROCESSOR, array, chiArrayField(array, idx), val);
}

bool chiArrayCas(Chili array, uint32_t idx, Chili expected, Chili desired) {
    return chiCasField(CHI_CURRENT_PROCESSOR, array, chiArrayField(array, idx), expected, desired);
}
