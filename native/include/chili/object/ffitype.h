_Static_assert(sizeof (float) == 4, "Invalid float size");
_Static_assert(sizeof (double) == 8, "Invalid double size");
_Static_assert(sizeof (char)  == 1, "Unsupported word size");
_Static_assert(sizeof (short) == 2, "Unsupported word size");
_Static_assert(sizeof (int) == 4, "Invalid int size");
_Static_assert(sizeof (long) == 4 || sizeof (long) == 8, "Invalid long size");
_Static_assert(sizeof (long long) == 8, "Invalid long long size");
_Static_assert(sizeof (intptr_t) == (CHI_ARCH_32BIT ? 4 : 8), "Invalid intptr_t size");
_Static_assert(sizeof (void*) == sizeof (intptr_t), "Invalid pointer size");
_Static_assert(sizeof (size_t) == sizeof (intptr_t), "Invalid size_t size");

CHI_INL CHI_WU Chili              chiFromLong(long v)                 { return sizeof (long) == 4 ? chiFromInt32(v)  : chiFromInt64(v);  }
CHI_INL CHI_WU long               chiToLong(Chili c)                  { return sizeof (long) == 4 ? chiToInt32(c)    : chiToInt64(c);    }
CHI_INL CHI_WU Chili              chiFromULong(unsigned long v)       { return sizeof (long) == 4 ? chiFromUInt32(v) : chiFromUInt64(v); }
CHI_INL CHI_WU unsigned long      chiToULong(Chili c)                 { return sizeof (long) == 4 ? chiToUInt32(c)   : chiToUInt64(c);   }

CHI_INL CHI_WU Chili              chiFromIntPtr(intptr_t v)           { return sizeof (intptr_t) == 4 ? chiFromInt32(v)  : chiFromInt64(v);  }
CHI_INL CHI_WU intptr_t           chiToIntPtr(Chili c)                { return sizeof (intptr_t) == 4 ? chiToInt32(c)    : chiToInt64(c);    }
CHI_INL CHI_WU Chili              chiFromUIntPtr(uintptr_t v)         { return sizeof (intptr_t) == 4 ? chiFromUInt32(v) : chiFromUInt64(v); }
CHI_INL CHI_WU uintptr_t          chiToUIntPtr(Chili c)               { return sizeof (intptr_t) == 4 ? chiToUInt32(c)   : chiToUInt64(c);   }

CHI_INL CHI_WU Chili              chiFromInt(int v)                   { return chiFromInt32(v);  }
CHI_INL CHI_WU Chili              chiFromLLong(long long v)           { return chiFromInt64(v);  }
CHI_INL CHI_WU Chili              chiFromSChar(char v)                { return chiFromInt8(v);   }
CHI_INL CHI_WU Chili              chiFromShort(short v)               { return chiFromInt16(v);  }
CHI_INL CHI_WU Chili              chiFromUChar(unsigned char v)       { return chiFromUInt8(v);  }
CHI_INL CHI_WU Chili              chiFromUInt(unsigned int v)         { return chiFromUInt32(v); }
CHI_INL CHI_WU Chili              chiFromULLong(unsigned long long v) { return chiFromUInt64(v); }
CHI_INL CHI_WU Chili              chiFromUShort(unsigned short v)     { return chiFromUInt16(v); }
CHI_INL CHI_WU char               chiToSChar(Chili c)                 { return chiToInt8(c);     }
CHI_INL CHI_WU int                chiToInt(Chili c)                   { return chiToInt32(c);    }
CHI_INL CHI_WU long long          chiToLLong(Chili c)                 { return chiToInt64(c);    }
CHI_INL CHI_WU short              chiToShort(Chili c)                 { return chiToInt16(c);    }
CHI_INL CHI_WU unsigned char      chiToUChar(Chili c)                 { return chiToUInt8(c);    }
CHI_INL CHI_WU unsigned int       chiToUInt(Chili c)                  { return chiToUInt32(c);   }
CHI_INL CHI_WU unsigned long long chiToULLong(Chili c)                { return chiToUInt64(c);   }
CHI_INL CHI_WU unsigned short     chiToUShort(Chili c)                { return chiToUInt16(c);   }

#define chiToCType(t, c)                              \
    _Generic(*(t*)0,                                  \
             float              : chiToFloat32,       \
             double             : chiToFloat64,       \
             int                : chiToInt,           \
             long               : chiToLong,          \
             long long          : chiToLLong,         \
             short              : chiToShort,         \
             char               : chiToSChar,         \
             unsigned int       : chiToInt,           \
             unsigned long      : chiToLong,          \
             unsigned long long : chiToLLong,         \
             unsigned short     : chiToShort,         \
             unsigned char      : chiToSChar,         \
             void*              : chiToPtr)(c)

#define chiFromCType(v)                               \
    _Generic((v),                                     \
             float              : chiFromFloat32,     \
             double             : chiFromFloat64,     \
             int                : chiFromInt,         \
             long               : chiFromLong,        \
             long long          : chiFromLLong,       \
             short              : chiFromShort,       \
             char               : chiFromSChar,       \
             unsigned int       : chiFromInt,         \
             unsigned long      : chiFromLong,        \
             unsigned long long : chiFromLLong,       \
             unsigned short     : chiFromShort,       \
             unsigned char      : chiFromSChar,       \
             void*              : chiFromPtr)(v)

#define CHI_PTR(t, n)                                   \
    CHI_EXPORT_INL CHI_WU t chiPtrPeek##n(const t* p) { return _chiPeek##n(p); } \
    CHI_EXPORT_INL void chiPtrPoke##n(t* p, t x) { _chiPoke##n(p, x); }
CHI_PTR(uint8_t, UInt8)
CHI_PTR(uint16_t, UInt16)
CHI_PTR(uint32_t, UInt32)
CHI_PTR(uint64_t, UInt64)
CHI_PTR(float, Float32)
CHI_PTR(double, Float64)
#undef CHI_PTR
