#define CHI_SIZED(bits, Type, type)                                     \
    CHI_INL CHI_WU Chili chiFrom##Type##bits(type##bits##_t v) {        \
        return chiFromUInt32((uint32_t)(type##32##_t)v);                \
    }                                                                   \
    CHI_INL CHI_WU type##bits##_t chiTo##Type##bits(Chili c) {          \
        return (type##bits##_t)chiToUInt32(c);                          \
    }
CHI_SIZED(8,  Int,  int)
CHI_SIZED(8,  UInt, uint)
CHI_SIZED(16, Int,  int)
CHI_SIZED(16, UInt, uint)
#undef CHI_SIZED
