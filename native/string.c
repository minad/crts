#include <chili/object/string.h>
#include "barrier.h"
#include "runtime.h"
#include "event.h"
#include "strutil.h"

bool chiUtf8Valid(const uint8_t* p, uint32_t n) {
    /*
     * Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
     * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
     * MIT License
     */
    static const uint8_t utf8table[] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3,11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
        0,12,24,36,60,96,84,12,12,12,48,72,12,12,12,12,12,12,12,12,12,12,12,12,
        12,0,12,12,12,12,12,0,12,0,12,12,12,24,12,12,12,12,12,24,12,24,12,12,
        12,12,12,12,12,12,12,24,12,12,12,12,12,24,12,12,12,12,12,12,12,24,12,12,
        12,12,12,12,12,12,12,36,12,36,12,12,12,36,12,12,12,12,12,36,12,36,12,12,
        12,36,12,12,12,12,12,12,12,12,12,12,
    };
    uint8_t state = 0;
    for (uint32_t i = 0; i < n; ++i)
        state = utf8table[256 + state + utf8table[p[i]]];
    return !state;
}

static Chili newStringFromBytes(const uint8_t* bytes, uint32_t size) {
    CHI_ASSERT(size);
    Chili c = chiStringNewFlags(size, CHI_NEW_UNINITIALIZED);
    memcpy(chiRawPayload(c), bytes, size);
    return c;
}

Chili chiCharToString(ChiChar c) {
    ChiWord w = 0;
    uint8_t* s = (uint8_t*)&w;
    s[0] = chiUtf8Encode(chiOrd(c), s + 1);
    return CHI_STRING_SMALLOPT ? chiFromUnboxed(w) : newStringFromBytes(s + 1, s[0]);
}

enum { TRAILING_SIZE_BYTE = 1 };

Chili chiStringNewFlags(uint32_t size, uint32_t flags) {
    CHI_ASSERT(size);
    Chili c = chiNewFlags(CHI_STRING, CHI_BYTES_TO_WORDS(size + TRAILING_SIZE_BYTE), flags);
    if (chiSuccess(c)) {
        uint8_t* bytes = (uint8_t*)chiRawPayload(c);
        if (!(flags & CHI_NEW_UNINITIALIZED))
            memset(bytes, 0, size);
        chiSetByteSize(bytes, size);
        CHI_ASSERT(chiStringSize(c) == size);
    }
    return c;
}

Chili chiStringTryNewPin(uint32_t size) {
    return chiStringNewFlags(size, CHI_NEW_TRY | CHI_NEW_PIN);
}

Chili chiSubstring(Chili a, uint32_t i, uint32_t j) {
    ChiStringRef s = chiStringRef(&a);
    CHI_ASSERT(i <= s.size);
    CHI_ASSERT(j <= s.size);
    CHI_ASSERT(j >= i);
    return chiStringFromBytes(s.bytes + i, j - i);
}

Chili chiStringFromBytes(const uint8_t* buf, uint32_t size) {
    CHI_ASSERT(chiUtf8Valid(buf, size));
    return chiFitsSmallString(size) ? chiFromSmallString(buf, size) : newStringFromBytes(buf, size);
}

Chili chiStaticString(const ChiStaticBytes* s) {
    CHI_ASSERT(s->size);
    CHI_ASSERT(!chiFitsSmallString(s->size));
    CHI_ASSERT(chiUtf8Valid(s->bytes, s->size));
    return newStringFromBytes(s->bytes, s->size);
}

bool chiStringRefEq(ChiStringRef a, ChiStringRef b) {
    return a.size == b.size && memeq(a.bytes, b.bytes, a.size);
}

bool chiStringEq(Chili a, Chili b) {
    return chiStringRefEq(chiStringRef(&a), chiStringRef(&b));
}

int32_t chiStringRefCmp(ChiStringRef a, ChiStringRef b) {
    const int cmp = memcmp(a.bytes, b.bytes, CHI_MIN(a.size, b.size));
    if (!cmp)
        return CHI_CMP(a.size, b.size);
    return cmp > 0 ? 1 : -1;
}

int32_t chiStringCmp(Chili a, Chili b) {
    return chiStringRefCmp(chiStringRef(&a), chiStringRef(&b));
}

Chili chiStringBuilderTryNew(uint32_t cap) {
    cap = CHI_MAX(cap, 32);
    Chili str = chiNewFlags(CHI_STRING, CHI_BYTES_TO_WORDS(cap + TRAILING_SIZE_BYTE), CHI_NEW_TRY);
    if (!chiSuccess(str))
        return CHI_FAIL;
    Chili b = chiNew(CHI_STRINGBUILDER, CHI_SIZEOF_WORDS(ChiStringBuilder));
    chiAtomicInit(&chiToStringBuilder(b)->string, str);
    chiToStringBuilder(b)->size = CHI_FALSE;
    return b;
}

static Chili builderOverflow(void) {
    CHI_EVENT0(CHI_CURRENT_PROCESSOR, STRBUILDER_OVERFLOW);
    return CHI_FAIL;
}

static Chili builderAppendBytes(Chili b, const uint8_t* bytes, uint32_t n) {
    ChiStringBuilder* sb = chiToStringBuilder(b);

    Chili str = chiAtomicLoad(&sb->string);
    uint32_t requestedCap, size = chiToUInt32(sb->size);

    // requested capacity too large
    if (CHI_UNLIKELY(__builtin_add_overflow(size, n, &requestedCap)))
        return builderOverflow();

    size_t cap = CHI_WORDSIZE * chiSize(str) - TRAILING_SIZE_BYTE;
    if (requestedCap > cap) {
        while (cap < requestedCap) {
            // new capacity too large
            if (CHI_UNLIKELY(__builtin_mul_overflow(cap, 2, &cap)))
                return builderOverflow();
        }

        // Allocate new string, return fail in case of failure
        Chili newStr = chiNewFlags(CHI_STRING, CHI_BYTES_TO_WORDS(cap + TRAILING_SIZE_BYTE), CHI_NEW_TRY);
        if (!chiSuccess(newStr))
            return CHI_FAIL;

        memcpy(chiRawPayload(newStr), chiRawPayload(str), size);
        chiWriteField(CHI_CURRENT_PROCESSOR, b, &sb->string, newStr);
        str = newStr;
    }

    memcpy((uint8_t*)chiRawPayload(str) + size, bytes, n);
    sb->size = chiFromUInt32(size + n);

    return b;
}

Chili chiStringBuilderTryChar(Chili b, ChiChar c) {
    uint8_t buf[6];
    return builderAppendBytes(b, buf, chiUtf8Encode(chiOrd(c), buf));
}

Chili chiStringBuilderTryString(Chili b, Chili s) {
    ChiStringRef r = chiStringRef(&s);
    return builderAppendBytes(b, r.bytes, r.size);
}

Chili chiStringBuilderBuild(Chili b) {
    ChiStringBuilder* sb = chiToStringBuilder(b);
    Chili str = chiAtomicLoad(&sb->string);
    uint32_t size = chiToUInt32(sb->size);
    uint8_t* bytes = (uint8_t*)chiRawPayload(str);

    // Unboxed small strings
    if (chiFitsSmallString(size))
        return chiFromSmallString(bytes, size);

    size_t allocW = chiSize(str), sizeW = CHI_BYTES_TO_WORDS(size + TRAILING_SIZE_BYTE);
    if (sizeW < allocW) {
        // Copy small unpinned strings to ensure tight packing on the major heap
        // after the string object gets promoted.
        if (allocW <= CHI_MAX_UNPINNED)
            return chiStringFromBytes(bytes, size);
        chiObjectShrink(chiObject(str), allocW, sizeW);
    }

    chiSetByteSize(bytes, size);
    CHI_ASSERT(chiStringSize(str) == size);
    return str;
}
