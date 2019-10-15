#define CHI_STATIC_BUFFER(s)                                            \
    ({                                                                  \
        static const ChiStaticBytes _staticBuffer = CHI_STATIC_BYTES(s); \
        _chiStaticBuffer(&_staticBuffer);                               \
    })

typedef struct CHI_WORD_ALIGNED { uint8_t bytes[1]; } ChiBuffer;

CHI_EXPORT CHI_WU Chili chiBufferNewFlags(uint32_t, uint8_t, ChiNewFlags);
CHI_EXPORT CHI_WU Chili chiBufferTryClone(Chili, uint32_t, uint32_t);
CHI_EXPORT CHI_WU Chili chiBufferFromBytes(const uint8_t*, uint32_t);
CHI_EXPORT CHI_WU Chili CHI_PRIVATE(chiStaticBuffer)(const ChiStaticBytes*);

CHI_INL CHI_WU ChiBuffer* CHI_PRIVATE(chiToBuffer)(Chili c) {
    CHI_ASSERT(_chiType(c) >= CHI_BUFFER0);
    return (ChiBuffer*)_chiRawPayload(c);
}

CHI_EXPORT_INL CHI_WU uint8_t* chiBufferBytes(Chili c) {
    return _chiToBuffer(c)->bytes;
}

CHI_INL CHI_WU uint32_t chiBufferSize(Chili c) {
    return CHI_WORDSIZE * (uint32_t)_chiSize(c) - (_chiType(c) - CHI_BUFFER0);
}

CHI_INL int32_t chiBufferCmp(Chili a, uint32_t aidx, Chili b, uint32_t bidx, uint32_t size) {
    return memcmp(chiBufferBytes(a) + aidx, chiBufferBytes(b) + bidx, size);
}

CHI_INL void chiBufferFill(Chili buf, uint8_t byte, uint32_t idx, uint32_t size) {
    memset(chiBufferBytes(buf) + idx, byte, size);
}

CHI_INL void chiBufferCopy(Chili src, uint32_t srcIdx, Chili dst, uint32_t dstIdx, uint32_t size) {
    memmove(chiBufferBytes(dst) + dstIdx, chiBufferBytes(src) + srcIdx, size);
}

#define CHI_BUFFER_FN(t, T)                                             \
    CHI_INL CHI_WU t chiBufferRead##T(Chili c, uint32_t i) {            \
        CHI_ASSERT(i + sizeof (t) <= chiBufferSize(c));                 \
        return _chiPeek##T(chiBufferBytes(c) + i);                      \
    }                                                                   \
    CHI_INL void chiBufferWrite##T(Chili c, uint32_t i, t x) {          \
        CHI_ASSERT(i + sizeof (x) <= chiBufferSize(c));                 \
        _chiPoke##T(chiBufferBytes(c) + i, x);                          \
    }
CHI_BUFFER_FN(uint8_t,  UInt8)
CHI_BUFFER_FN(uint16_t, UInt16)
CHI_BUFFER_FN(uint32_t, UInt32)
CHI_BUFFER_FN(uint64_t, UInt64)
CHI_BUFFER_FN(float,    Float32)
CHI_BUFFER_FN(double,   Float64)
#undef CHI_BUFFER_FN
