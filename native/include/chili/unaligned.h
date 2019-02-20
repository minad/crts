#define CHI_UNALIGNED(t, peek, poke)                    \
    CHI_INL CHI_WU t peek(const void* p) {              \
        t x; memcpy(&x, p, sizeof (x)); return x;       \
    }                                                   \
    CHI_INL void poke(void* p, t x) {                   \
        memcpy(p, &x, sizeof (x));                      \
    }
CHI_UNALIGNED(uint8_t,  CHI_PRIVATE(chiPeekUInt8),   CHI_PRIVATE(chiPokeUInt8))
CHI_UNALIGNED(uint16_t, CHI_PRIVATE(chiPeekUInt16),  CHI_PRIVATE(chiPokeUInt16))
CHI_UNALIGNED(uint32_t, CHI_PRIVATE(chiPeekUInt32),  CHI_PRIVATE(chiPokeUInt32))
CHI_UNALIGNED(uint64_t, CHI_PRIVATE(chiPeekUInt64),  CHI_PRIVATE(chiPokeUInt64))
CHI_UNALIGNED(size_t,   CHI_PRIVATE(chiPeekSize),    CHI_PRIVATE(chiPokeSize))
CHI_UNALIGNED(float,    CHI_PRIVATE(chiPeekFloat32), CHI_PRIVATE(chiPokeFloat32))
CHI_UNALIGNED(double,   CHI_PRIVATE(chiPeekFloat64), CHI_PRIVATE(chiPokeFloat64))
#undef CHI_UNALIGNED
