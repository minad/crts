#pragma once

#include "bytes.h"

enum { CHI_SMALL_STRING_MAX = 7 };

#define CHI_STRINGREF(s)    ((ChiStringRef){ .size = CHI_STRSIZE(s), .bytes = (const uint8_t*)(s) })
#define CHI_EMPTY_STRINGREF CHI_STRINGREF("")

#define CHI_STATIC_STRING(s)                                           \
    ({                                                                 \
        static const ChiStaticBytes _staticString = CHI_STATIC_BYTES(s);       \
        _Static_assert(CHI_STRSIZE(s) > 0, "String must not be empty"); \
        _chiFitsSmallString(CHI_STRSIZE(s))                            \
            ? _chiFromSmallString((const uint8_t*)(s), CHI_STRSIZE(s)) \
            : _chiStaticString(&_staticString);                        \
    })

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
 * ChiBytesRef      Reference to bytes
 */

CHI_OBJECT(STRINGBUILDER, StringBuilder, Chili size; ChiAtomic string)

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
    uint32_t       size;
    const uint8_t* bytes;
} ChiStringRef;

CHI_NEWTYPE(Char, uint32_t)

// C11 _Generic could be used here instead, but it is incompatible with Coverity and Sparse
/*
#define chiStringRef(x)                                         \
    _Generic((x),                                               \
             Chili*            : _chiToStringRef,               \
             const Chili*      : _chiToStringRef,               \
             char*             : _chiCStringToStringRef,        \
             const char*       : _chiCStringToStringRef,        \
             default           : _chiStringRefToStringRef)(x)
*/
#define chiStringRef(x)                                         \
    (__builtin_choose_expr(__builtin_types_compatible_p(typeof(x), Chili*),           _chiToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), const Chili*),     _chiToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), char*),            _chiCStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), const char*),      _chiCStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), char[]),           _chiCStringToStringRef, \
     __builtin_choose_expr(__builtin_types_compatible_p(typeof(x), const char[]),     _chiCStringToStringRef, \
                           _chiStringRefToStringRef))))))(x))

#define chiStringNew(x)    _chiFromStringRef(chiStringRef(x))

CHI_API CHI_WU Chili     chiCharToString(ChiChar);
CHI_API CHI_WU Chili     chiStringNewFlags(uint32_t, uint32_t);
CHI_API CHI_WU Chili     chiStringTryNewPin(uint32_t);
CHI_API CHI_WU bool      chiStringEq(Chili, Chili);
CHI_API CHI_WU bool      chiStringRefEq(ChiStringRef, ChiStringRef);
CHI_API CHI_WU int32_t   chiStringCmp(Chili, Chili);
CHI_API CHI_WU int32_t   chiStringRefCmp(ChiStringRef, ChiStringRef);
CHI_API CHI_WU Chili     chiStringFromBytes(const uint8_t*, uint32_t);
CHI_API CHI_WU Chili     chiStringBuilderTryNew(uint32_t);
CHI_API CHI_WU Chili     chiStringBuilderTryChar(Chili, ChiChar);
CHI_API CHI_WU Chili     chiStringBuilderTryString(Chili, Chili);
CHI_API CHI_WU Chili     chiStringBuilderBuild(Chili);
CHI_API CHI_WU Chili     chiTryPinString(Chili);
CHI_API CHI_WU bool      chiUtf8Valid(const uint8_t*, uint32_t);
CHI_API CHI_WU Chili     chiSubstring(Chili, uint32_t, uint32_t);
CHI_API CHI_WU Chili     CHI_PRIVATE(chiStaticString)(const ChiStaticBytes*);

CHI_API_INL CHI_WU ChiChar chiChr(uint32_t c) {
    return (ChiChar){ c };
}

CHI_API_INL CHI_WU uint32_t chiOrd(ChiChar c) {
    return CHI_UN(Char, c);
}

CHI_INL uint32_t CHI_PRIVATE(chiUtf8DecodeSize)(uint8_t c) {
    static const uint64_t utf8count = 0x4322000011111111ULL;
    return (uint32_t)((utf8count >> ((c >> 4) << 2)) & 7);
}

CHI_INL uint32_t CHI_PRIVATE(chiUtf8Decode)(const uint8_t** p, const uint8_t* end) {
    const uint8_t* s = *p;
    CHI_ASSERT(s < end);
    uint32_t n = _chiUtf8DecodeSize(*s);
    CHI_ASSERT(s + n <= end);
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

CHI_INL CHI_WU ChiStringRef _chiToStringRef(const Chili* c) {
    if (CHI_STRING_SMALLOPT && _chiUnboxed(*c)) {
        const uint8_t* s = (const uint8_t*)c;
        CHI_ASSERT(s[0] <= CHI_SMALL_STRING_MAX);
        return (ChiStringRef) { .bytes = s + 1, .size = s[0] };
    }
    if (!CHI_STRING_SMALLOPT && !chiTrue(*c))
        return CHI_EMPTY_STRINGREF;
    const uint8_t* bytes = (const uint8_t*)_chiRawPayload(*c);
    return (ChiStringRef){ .bytes = bytes, .size = _chiByteSize(bytes, chiSize(*c)) };
}

CHI_INL CHI_WU ChiStringRef _chiCStringToStringRef(const char* s) {
    CHI_ASSERT(s);
    return (ChiStringRef){ .size = (uint32_t)strlen(s), .bytes = (const uint8_t*)s };
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiFitsSmallString)(uint32_t size) {
    return CHI_STRING_SMALLOPT ? size <= CHI_SMALL_STRING_MAX : !size;
}

CHI_INL CHI_WU Chili CHI_PRIVATE(chiFromSmallString)(const uint8_t* bytes, uint32_t size) {
    if (!CHI_STRING_SMALLOPT) {
        if (!size)
            return CHI_FALSE;
        CHI_BUG("CHI_STRING_SMALLOPT is disabled");
    }
    CHI_BOUNDED(size, CHI_SMALL_STRING_MAX);
    ChiWord w = 0;
    uint8_t* s = (uint8_t*)&w;
    s[0] = (uint8_t)size;
    memcpy(s + 1, bytes, size);
    return _chiFromUnboxed(w);
}

CHI_INL CHI_WU Chili _chiFromStringRef(ChiStringRef s) {
    return chiStringFromBytes(s.bytes, s.size);
}

CHI_API_INL CHI_WU ChiBytesRef chiStringToBytesRef(ChiStringRef ref) {
    return (ChiBytesRef){ .size = ref.size, .bytes = ref.bytes }; // safe. we only forget the encoding.
}

CHI_API_INL CHI_WU const uint8_t* chiPinnedStringBytes(Chili c) {
    CHI_ASSERT(!chiTrue(c) || chiPinned(c));
    return chiTrue(c) ? (const uint8_t*)_chiRawPayload(c) : (const uint8_t*)"";
}

CHI_API_INL CHI_WU const uint8_t* chiStringBytes(const Chili* c) {
    return chiStringRef(c).bytes;
}

CHI_API_INL CHI_WU uint32_t chiStringSize(Chili c) {
    return chiStringRef(&c).size;
}

CHI_API_INL CHI_WU Chili chiFromChar(ChiChar c) {
    return chiFromUInt32(chiOrd(c));
}

CHI_API_INL CHI_WU ChiChar chiToChar(Chili c) {
    return chiChr(chiToUInt32(c));
}

CHI_API_INL uint32_t chiStringNext(Chili s, uint32_t i) {
    const ChiStringRef r = chiStringRef(&s);
    CHI_ASSERT(i < r.size);
    i += _chiUtf8DecodeSize(r.bytes[i]);
    CHI_ASSERT(i <= r.size);
    return i;
}

CHI_API_INL uint32_t chiStringPrev(Chili s, uint32_t i) {
    const ChiStringRef r = chiStringRef(&s);
    do {
        CHI_ASSERT(i > 0);
        --i;
    } while ((r.bytes[i] & 0xC0) == 0x80);
    return i;
}

CHI_API_INL ChiChar chiStringGet(Chili c, uint32_t i) {
    const ChiStringRef r = chiStringRef(&c);
    const uint8_t* s = r.bytes + i;
    return chiChr(_chiUtf8Decode(&s, r.bytes + r.size));
}
