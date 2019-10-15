#include "new.h"

Chili chiBufferNewUninitialized(ChiProcessor* proc, uint32_t size, ChiNewFlags flags) {
    if (!size)
        return chiNewEmpty(CHI_BUFFER0);
    uint32_t delta = size % CHI_WORDSIZE == 0 ? 0 : CHI_WORDSIZE - size % CHI_WORDSIZE;
    Chili c = chiNewInl(proc, (ChiType)(CHI_BUFFER0 + delta), CHI_BYTES_TO_WORDS(size),
                        flags | CHI_NEW_SHARED | CHI_NEW_CLEAN);
    CHI_ASSERT(!chiSuccess(c) || chiBufferSize(c) == size);
    return c;
}

Chili chiBufferNewFlags(uint32_t size, uint8_t val, ChiNewFlags flags) {
    if (!size)
        return chiNewEmpty(CHI_BUFFER0);
    Chili c = chiBufferNewUninitialized(CHI_CURRENT_PROCESSOR, size, flags);
    if (chiSuccess(c) && !(flags & CHI_NEW_UNINITIALIZED))
        memset(chiBufferBytes(c), val, size);
    return c;
}

Chili chiBufferTryClone(Chili src, uint32_t start, uint32_t size) {
    if (!size)
        return chiNewEmpty(CHI_BUFFER0);
    Chili c = chiBufferNewFlags(size, 0, CHI_NEW_UNINITIALIZED | CHI_NEW_TRY);
    if (chiSuccess(c))
        memcpy(chiBufferBytes(c), chiBufferBytes(src) + start, size);
    return c;
}

Chili chiBufferFromBytes(const uint8_t* bytes, uint32_t size) {
    Chili c = chiBufferNewFlags(size, 0, CHI_NEW_UNINITIALIZED);
    memcpy(chiBufferBytes(c), bytes, size);
    return c;
}

Chili chiStaticBuffer(const ChiStaticBytes* b) {
    return chiBufferFromBytes(b->bytes, b->size);
}
