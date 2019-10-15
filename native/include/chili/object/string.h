enum { CHI_SMALL_STRING_MAX = CHI_WORDSIZE - 1 };

#define CHI_STRINGREF(s)    ((ChiStringRef){ .size = CHI_STRSIZE(s), .bytes = (const uint8_t*)(s) })
#define CHI_EMPTY_STRINGREF CHI_STRINGREF("")

typedef ChiStaticBytes ChiStaticString;

#define CHI_STATIC_STRING(s)                                           \
    ({                                                                 \
        static const ChiStaticString _staticString = CHI_STATIC_BYTES(s); \
        _Static_assert(CHI_STRSIZE(s) > 0, "String must not be empty"); \
        &_staticString;                                                 \
    })

CHI_OBJECT(STRING, String, uint8_t bytes[1])
CHI_OBJECT(PRESTRING, PreString, uint8_t bytes[1])

/*
 * There multiple string related types:
 *
 * char*            Zero terminated C string
 * ChiStringRef     String reference to unify access to all the different string types
 *
 * ChiStringBuilder StringBuilder object allocated on the Chili heap
 * ChiChar          Unicode character
 *
 * ChiStaticBytes   Static bytes array
 * ChiStaticString  Static string type alias (same as ChiStaticBytes)
 */

CHI_OBJECT(STRINGBUILDER, StringBuilder, Chili cap; Chili size; ChiField buf)

/**
 * Reference to string buffer with explicit size field.
 * The string must be UTF-8 encoded.
 * We pass ChiStringRef generally by value when calling inlined functions.
 * If function should work with Chili strings and zero-terminated strings,
 * a ChiStringRef should be used.
 *
 * @see chiStringRef
 */
typedef struct {
    const uint8_t* bytes;
    uint32_t       size;
} ChiStringRef;

CHI_NEWTYPE(Char, uint32_t)

// C11 _Generic could be used here instead, but it is incompatible with Coverity and Sparse
/*
#define chiStringRef(x)                                         \
    _Generic((x),                                               \
             Chili*            : _chiToStringRef,         \
             const Chili*      : _chiToStringRef,         \
             char*             : _chiCStringToStringRef,        \
             const char*       : _chiCStringToStringRef,        \
             default           : _chiStringRefToStringRef)(x)
*/
#define chiStringRef(x)                                         \
    (__builtin_choose_expr(__builtin_types_compatible_p(typeof(x), Chili*),                 _chiToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), const Chili*),           _chiToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), ChiStaticString*),       _chiStaticStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), const ChiStaticString*), _chiStaticStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), char*),                  _chiCStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), const char*),            _chiCStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), char[]),                 _chiCStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), const char[]),           _chiCStringToStringRef, \
                           _chiStringRefToStringRef))))))))(x))

CHI_EXPORT CHI_WU Chili     chiStringNewFlags(uint32_t, ChiNewFlags);
CHI_EXPORT CHI_WU bool      chiStringEq(Chili, Chili);
CHI_EXPORT CHI_WU bool      chiStringRefEq(ChiStringRef, ChiStringRef);
CHI_EXPORT CHI_WU int32_t   chiStringCmp(Chili, Chili);
CHI_EXPORT CHI_WU int32_t   chiStringRefCmp(ChiStringRef, ChiStringRef);
CHI_EXPORT CHI_WU Chili     chiStringBuilderTryNew(uint32_t);
CHI_EXPORT CHI_WU Chili     chiStringFromRef(ChiStringRef);
CHI_EXPORT CHI_WU Chili     chiStringBuilderBuild(Chili);
CHI_EXPORT CHI_WU Chili     chiStringPin(Chili);
CHI_EXPORT CHI_WU Chili     chiStringTrySlice(Chili, uint32_t, uint32_t);
CHI_EXPORT CHI_WU bool      chiUtf8Valid(const uint8_t*, uint32_t);
CHI_EXPORT CHI_WU bool      CHI_PRIVATE(chiStringBuilderTryGrow)(Chili, uint32_t);
CHI_EXPORT CHI_WU Chili     CHI_PRIVATE(chiStaticString)(const ChiStaticString*);

CHI_INL CHI_WU ChiChar chiChr(uint32_t c) {
    return CHI_WRAP(Char, c);
}

CHI_INL CHI_WU uint32_t chiOrd(ChiChar c) {
    return CHI_UN(Char, c);
}

CHI_INL uint32_t CHI_PRIVATE(chiUtf8DecodeSize)(uint8_t c) {
    static const uint64_t utf8count = UINT64_C(0x4322000011111111);
    return (uint32_t)((utf8count >> ((c >> 4) << 2)) & 7);
}

CHI_INL uint32_t CHI_PRIVATE(chiUtf8Decode)(const uint8_t** p, const uint8_t* end) {
    const uint8_t* s = *p;
    CHI_ASSERT(s < end);
    uint32_t n = _chiUtf8DecodeSize(*s);
    CHI_ASSERT(s + n <= end); CHI_NOWARN_UNUSED(end);
    *p = s + n;
    switch (n) {
    case 1: return s[0];
    case 2: return ((s[0] & 31U) << 6)
            | (s[1] & 0x3FU);
    case 3: return ((s[0] & 15U) << 12)
            | ((s[1] & 0x3FU) << 6)
            | (s[2] & 0x3FU);
    case 4: return ((s[0] & 7U) << 18)
            | ((s[1] & 0x3FU) << 12)
            | ((s[2] & 0x3FU) << 6)
            | (s[3] & 0x3FU);
    }
    return 0;
}

CHI_INL uint8_t CHI_PRIVATE(chiUtf8EncodeSize)(uint32_t c) {
    if (c <= 0x7F)
        return 1;
    if (c <= 0x7FF)
        return 2;
    if (c <= 0xFFFF)
        return 3;
    if (c <= 0x1FFFFF)
        return 4;
    return 0;
}

CHI_INL uint8_t CHI_PRIVATE(chiUtf8Encode)(uint32_t c, uint8_t* p) {
    if (c <= 0x7F) {
        p[0] = (uint8_t)c;
        return 1;
    }
    if (c <= 0x7FF) {
        p[0] = (uint8_t) (0xC0 | (c >> 6));
        p[1] = (uint8_t) (0x80 | (c & 0x3F));
        return 2;
    }
    if (c <= 0xFFFF) {
        p[0] = (uint8_t) (0xE0 | (c >> 12));
        p[1] = (uint8_t) (0x80 | ((c >> 6) & 0x3F));
        p[2] = (uint8_t) (0x80 | (c & 0x3F));
        return 3;
    }
    if (c <= 0x1FFFFF) {
        p[0] = (uint8_t) (0xF0 | (c >> 18));
        p[1] = (uint8_t) (0x80 | ((c >> 12) & 0x3F));
        p[2] = (uint8_t) (0x80 | ((c >> 6) & 0x3F));
        p[3] = (uint8_t) (0x80 | (c & 0x3F));
        return 4;
    }
    return 0;
}

CHI_INL CHI_WU ChiStringRef _chiStringRefToStringRef(ChiStringRef s) {
    return s;
}

CHI_INL CHI_WU ChiStringRef _chiStaticStringToStringRef(const ChiStaticString* s) {
    return (ChiStringRef){ .bytes = s->bytes, .size = s->size };
}

CHI_INL CHI_WU uint32_t CHI_PRIVATE(chiStringTrailSize)(const uint8_t* bytes, size_t sizeW) {
    CHI_ASSERT(sizeW);
    uint32_t pad = bytes[sizeW * CHI_WORDSIZE - 1];
    CHI_ASSERT(pad < CHI_WORDSIZE);
    return CHI_WORDSIZE * (uint32_t)sizeW - 1 - pad;
}

CHI_INL CHI_WU ChiStringRef _chiToStringRef(const Chili* c) {
    if (CHI_STRING_UNBOXING && _chiUnboxed63(*c)) {
        const uint8_t* s = (const uint8_t*)c;
        CHI_ASSERT(s[CHI_SMALL_STRING_MAX] <= CHI_SMALL_STRING_MAX);
        return (ChiStringRef) { .bytes = s, .size = s[CHI_SMALL_STRING_MAX] };
    }
    if (!CHI_STRING_UNBOXING && !chiTrue(*c))
        return CHI_EMPTY_STRINGREF;
    const uint8_t* bytes = _chiToString(*c)->bytes;
    return (ChiStringRef){ .bytes = bytes, .size = _chiStringTrailSize(bytes, _chiSize(*c)) };
}

CHI_INL CHI_WU ChiStringRef _chiCStringToStringRef(const char* s) {
    CHI_ASSERT(s);
    return (ChiStringRef){ .size = (uint32_t)strlen(s), .bytes = (const uint8_t*)s };
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiFitsSmallString)(uint32_t size) {
    return CHI_STRING_UNBOXING ? size <= CHI_SMALL_STRING_MAX : !size;
}

CHI_INL CHI_WU Chili CHI_PRIVATE(chiFromSmallString)(const uint8_t* bytes, uint32_t size) {
    if (!CHI_STRING_UNBOXING) {
        if (!size)
            return CHI_FALSE;
        CHI_BUG("CHI_STRING_UNBOXING is disabled");
    }
    CHI_ASSUME(size <= CHI_SMALL_STRING_MAX);
    ChiWord w = (ChiWord)size << 56;
    CHI_ASSERT(((uint8_t*)&w)[CHI_SMALL_STRING_MAX] == size); // Little endian
    for (uint32_t i = 0; i < CHI_SMALL_STRING_MAX; ++i) {
        if (size) {
            --size;
            w |= (ChiWord)bytes[i] << (8 * i);
            CHI_ASSERT(((uint8_t*)&w)[i] == bytes[i]);
        }
    }
    CHI_ASSERT(size == 0);
    return _chiFromUnboxed(w);
}

CHI_EXPORT_INL CHI_WU const uint8_t* chiStringPinnedBytes(Chili c) {
    CHI_ASSERT(!chiTrue(c) || _chiGen(c) == CHI_GEN_MAJOR);
    return chiTrue(c) ? _chiToString(c)->bytes : (const uint8_t*)"";
}

CHI_INL CHI_WU const uint8_t* chiStringBytes(const Chili* c) {
    return chiStringRef(c).bytes;
}

// Helper function which can be used by the FFI for unpinned strings
CHI_EXPORT_INL void chiStringMemcpy(Chili src, uint32_t srcIdx, uint8_t* dst, uint32_t size) {
    memcpy(dst, chiStringBytes(&src) + srcIdx, size);
}

CHI_EXPORT_INL CHI_WU uint32_t chiStringSize(Chili c) {
    return chiStringRef(&c).size;
}

CHI_INL CHI_WU Chili chiFromChar(ChiChar c) {
    return chiFromUInt32(chiOrd(c));
}

CHI_INL CHI_WU ChiChar chiToChar(Chili c) {
    return chiChr(chiToUInt32(c));
}

CHI_INL CHI_WU Chili chiStringFromBytes(const uint8_t* b, uint32_t n) {
    return chiStringFromRef((ChiStringRef){ .bytes = b, .size = n });
}

CHI_INL Chili chiCharToString(ChiChar c) {
    ChiWord w = 0;
    uint8_t* s = (uint8_t*)&w;
    uint8_t n = _chiUtf8Encode(chiOrd(c), s);
    s[CHI_SMALL_STRING_MAX] = n;
    return CHI_STRING_UNBOXING ? _chiFromUnboxed(w) : chiStringFromBytes(s, n);
}

CHI_INL uint32_t chiStringNext(Chili s, uint32_t i) {
    const ChiStringRef r = chiStringRef(&s);
    CHI_ASSERT(i < r.size);
    i += _chiUtf8DecodeSize(r.bytes[i]);
    CHI_ASSERT(i <= r.size);
    return i;
}

CHI_INL uint32_t chiStringPrev(Chili s, uint32_t i) {
    const ChiStringRef r = chiStringRef(&s);
    do {
        CHI_ASSERT(i > 0);
        --i;
    } while ((r.bytes[i] & 0xC0) == 0x80);
    return i;
}

CHI_INL ChiChar chiStringGet(Chili c, uint32_t i) {
    const ChiStringRef r = chiStringRef(&c);
    const uint8_t* s = r.bytes + i;
    return chiChr(_chiUtf8Decode(&s, r.bytes + r.size));
}

CHI_INL bool chiStringBuilderEnsure(Chili b, uint32_t n) {
    ChiStringBuilder* sb = _chiToStringBuilder(b);
    return (uint64_t)chiToUInt32(sb->size) + n <= chiToUInt32(sb->cap) || _chiStringBuilderTryGrow(b, n);
}

CHI_INL void _chiStringBuilderAppend(Chili b, const uint8_t* bytes, uint32_t n) {
    ChiStringBuilder* sb = _chiToStringBuilder(b);
    uint32_t size = chiToUInt32(sb->size);
    CHI_ASSERT((uint64_t)size + n <= chiToUInt32(sb->cap));
    memcpy(_chiToPreString(_chiFieldRead(&sb->buf))->bytes + size, bytes, n);
    sb->size = chiFromUInt32(size + n);
}

CHI_INL void chiStringBuilderChar(Chili b, ChiChar c) {
    uint8_t buf[4];
    _chiStringBuilderAppend(b, buf, _chiUtf8Encode(chiOrd(c), buf));
}

CHI_INL void chiStringBuilderString(Chili b, ChiStringRef s) {
    _chiStringBuilderAppend(b, s.bytes, s.size);
}

CHI_INL CHI_WU Chili chiFromStaticString(const ChiStaticString* s) {
    CHI_ASSERT(s->size != 0);
    return _chiFitsSmallString(s->size)
        ? _chiFromSmallString(s->bytes, s->size)
        : _chiStaticString(s);
}
