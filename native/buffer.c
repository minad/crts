#include "private.h"
#include <chili/object/buffer.h>

Chili chiBufferNewFlags(uint32_t size, uint8_t val, uint32_t flags) {
    CHI_ASSERT(size);
    Chili c = chiNewFlags(CHI_BUFFER, CHI_BYTES_TO_WORDS(size + 1), flags);
    if (chiSuccess(c)) {
        uint8_t* bytes = chiBufferBytes(c);
        if (!(flags & CHI_NEW_UNINITIALIZED))
            memset(bytes, val, size);
        chiSetByteSize(bytes, size);
        CHI_ASSERT(chiBufferSize(c) == size);
    }
    return c;
}

Chili chiBufferTryNew(uint32_t size, uint8_t val) {
    return chiBufferNewFlags(size, val, CHI_NEW_TRY);
}

Chili chiBufferTryNewPin(uint32_t size, uint8_t val) {
    return chiBufferNewFlags(size, val, CHI_NEW_TRY | CHI_NEW_PIN);
}

Chili chiBufferTryClone(Chili src, uint32_t start, uint32_t size) {
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
