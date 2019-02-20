/* -*- mode: text -*- */
#define CHI_FOREACH_FFI_TYPE(SEP, TYPE)                                                                                                                \
    TYPE(Void,    void,    CHI_NOWARN_UNUSED(f),                    ({dcCallVoid(v,f);(ChiVoid){0};}),      ChiVoid)        SEP \
    TYPE(Int8,    sint8,   dcArgChar(v,f),                          dcCallChar(v,f),                        int8_t)         SEP \
    TYPE(UInt8,   uint8,   dcArgChar(v,(char)f),                    (uint8_t)dcCallChar(v,f),               uint8_t)        SEP \
    TYPE(Int16,   sint16,  dcArgShort(v,f),                         dcCallShort(v,f),                       int16_t)        SEP \
    TYPE(UInt16,  uint16,  dcArgShort(v,(short)f),                  (uint16_t)dcCallShort(v,f),             uint16_t)       SEP \
    TYPE(Int32,   sint32,  dcArgInt(v,f),                           dcCallInt(v,f),                         int32_t)        SEP \
    TYPE(UInt32,  uint32,  dcArgInt(v,(int)f),                      (uint32_t)dcCallInt(v,f),               uint32_t)       SEP \
    TYPE(Int64,   sint64,  dcArgLongLong(v,f),                      dcCallLongLong(v,f),                    int64_t)        SEP \
    TYPE(UInt64,  uint64,  dcArgLongLong(v,(long long)f),           (uint64_t)dcCallLongLong(v,f),          uint64_t)       SEP \
    TYPE(Float32, float,   dcArgFloat(v,f),                         dcCallFloat(v,f),                       float)          SEP \
    TYPE(Float64, double,  dcArgDouble(v,f),                        dcCallDouble(v,f),                      double)         SEP \
    TYPE(UChar,   uchar,   dcArgChar(v,(char)f),                    (unsigned char)dcCallChar(v,f),         unsigned char)  SEP \
    TYPE(SChar,   schar,   dcArgChar(v,f),                          dcCallChar(v,f),                        char)           SEP \
    TYPE(UShort,  ushort,  dcArgShort(v,(short)f),                  (unsigned short)dcCallShort(v,f),       unsigned short) SEP \
    TYPE(Short,   sshort,  dcArgShort(v,f),                         dcCallShort(v,f),                       short)          SEP \
    TYPE(UInt,    uint,    dcArgInt(v,(int)f),                      (unsigned int)dcCallInt(v,f),           unsigned int)   SEP \
    TYPE(Int,     sint,    dcArgInt(v,f),                           dcCallInt(v,f),                         int)            SEP \
    TYPE(ULong,   ulong,   dcArgLong(v,(long)f),                    (unsigned long)dcCallLong(v,f),         unsigned long)  SEP \
    TYPE(Long,    slong,   dcArgLong(v,f),                          dcCallLong(v,f),                        long)           SEP \
    TYPE(UIntPtr, pointer, dcArgPointer(v,(void*)f),                (uintptr_t)dcCallPointer(v,f),          uintptr_t)      SEP \
    TYPE(IntPtr,  pointer, dcArgPointer(v,(void*)f),                (intptr_t)dcCallPointer(v,f),           intptr_t)       SEP \
    TYPE(Ptr,     pointer, dcArgPointer(v,f),                       dcCallPointer(v,f),                     void*)          SEP \
    TYPE(Bool,    uint8,   dcArgBool(v,f),                          dcCallBool(v,f),                        bool)           SEP \
    TYPE(Char,    uint32,  dcArgInt(v,(int)f),                      (uint32_t)dcCallInt(v,f),               ChiChar)        SEP \
    TYPE(Chili,   uint64,  dcArgLongLong(v,(long long)CHILI_UN(f)), (Chili){(uint64_t)dcCallLongLong(v,f)}, Chili)
