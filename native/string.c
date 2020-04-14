#include "barrier.h"
#include "event.h"
#include "new.h"
#include "strutil.h"

enum { TRAILING_SIZE_BYTE = 1 };

// Store bytesize in trailing byte like Ocaml does it.
CHI_INL void setTrailSize(uint8_t* bytes, uint32_t size) {
    CHI_ASSERT(size != 0);
    uint8_t pad = (uint8_t)(CHI_WORDSIZE - 1 - (size & (CHI_WORDSIZE - 1)));
    bytes[size + pad] = pad;
    for (uint32_t i = 0; i < CHI_WORDSIZE - 1; ++i) {
        if (pad) {
            --pad;
            bytes[size + i] = 0;
        }
    }
    CHI_ASSERT(chiStringTrailSize(bytes, CHI_BYTES_TO_WORDS(size + TRAILING_SIZE_BYTE)) == size);
}

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

static Chili newStringUninitialized(ChiProcessor* proc, uint32_t size, ChiNewFlags flags) {
    Chili c = chiNewInl(proc, CHI_STRING, CHI_BYTES_TO_WORDS(size + TRAILING_SIZE_BYTE), flags);
    if (chiSuccess(c)) {
        setTrailSize(chiToString(c)->bytes, size);
        CHI_ASSERT(chiStringSize(c) == size);
    }
    return c;
}

static Chili newStringFromBytes(ChiProcessor* proc, const uint8_t* bytes, uint32_t size, ChiNewFlags flags) {
    CHI_ASSERT(size);
    Chili c = newStringUninitialized(proc, size, flags);
    if (chiSuccess(c))
        memcpy(chiToString(c)->bytes, bytes, size);
    return c;
}

Chili chiStringNewFlags(uint32_t size, ChiNewFlags flags) {
    if (!size)
        return CHI_FALSE;
    Chili c = newStringUninitialized(CHI_CURRENT_PROCESSOR, size, flags);
    if (chiSuccess(c) && !(flags & CHI_NEW_UNINITIALIZED))
        memset(chiToString(c)->bytes, 0, size);
    return c;
}

Chili chiStringTrySlice(Chili a, uint32_t i, uint32_t j) {
    ChiStringRef s = chiStringRef(&a);
    CHI_ASSERT(i <= s.size);
    CHI_ASSERT(j <= s.size);
    CHI_ASSERT(j >= i);
    return chiStringFromBytesFlags(CHI_CURRENT_PROCESSOR, s.bytes + i, j - i, CHI_NEW_TRY);
}

Chili chiStringFromBytesFlags(ChiProcessor* proc, const uint8_t* b, uint32_t n, ChiNewFlags flags) {
    CHI_ASSERT(!(flags & (CHI_NEW_LOCAL | CHI_NEW_SHARED | CHI_NEW_UNINITIALIZED))); // disallowed flags
    CHI_ASSERT(chiUtf8Valid(b, n));
    return chiFitsSmallString(n) ? chiFromSmallString(b, n)
        : newStringFromBytes(proc, b, n, flags);
}

Chili chiStringFromBytes(const uint8_t* b, uint32_t n) {
    return chiStringFromBytesFlags(CHI_CURRENT_PROCESSOR, b, n, CHI_NEW_DEFAULT);
}

Chili chiStaticString(const ChiStaticBytes* s) {
    CHI_ASSERT(s->size);
    CHI_ASSERT(!chiFitsSmallString(s->size));
    CHI_ASSERT(chiUtf8Valid(s->bytes, s->size));
    return newStringFromBytes(CHI_CURRENT_PROCESSOR, s->bytes, s->size, CHI_NEW_DEFAULT);
}

Chili chiStringBuilderTryNew(uint32_t cap) {
    cap = CHI_MAX(cap, 32);
    Chili str = chiNewFlags(CHI_PRESTRING, CHI_BYTES_TO_WORDS(cap + TRAILING_SIZE_BYTE), CHI_NEW_TRY);
    if (!chiSuccess(str))
        return CHI_FAIL;
    Chili b = chiNew(CHI_STRINGBUILDER, CHI_SIZEOF_WORDS(ChiStringBuilder));
    chiFieldInit(&chiToStringBuilder(b)->buf, str);
    chiToStringBuilder(b)->size = CHI_FALSE;
    chiToStringBuilder(b)->cap = chiFromUInt32(cap);
    return b;
}

static bool builderOverflow(ChiProcessor* proc) {
    chiEvent0(proc, STRBUILDER_OVERFLOW);
    return false;
}

bool chiStringBuilderTryGrow(Chili b, uint32_t n) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiStringBuilder* sb = chiToStringBuilder(b);

    uint64_t cap = chiToUInt32(sb->cap);
    while ((uint64_t)chiToUInt32(sb->size) + n > cap)
        cap *= 2;

    // new capacity too large
    if (CHI_UNLIKELY(cap >= UINT32_MAX))
        return builderOverflow(proc);

    // Allocate new string, return fail in case of failure
    Chili newBuf = chiNewInl(proc, CHI_PRESTRING,
                                CHI_BYTES_TO_WORDS((uint32_t)cap + TRAILING_SIZE_BYTE), CHI_NEW_TRY);
    if (!chiSuccess(newBuf))
        return builderOverflow(proc);

    memcpy(chiToPreString(newBuf)->bytes,
           chiToPreString(chiFieldRead(&sb->buf))->bytes,
           chiToUInt32(sb->size));
    chiFieldWrite(proc, b, &sb->buf, newBuf);
    sb->cap = chiFromUInt32((uint32_t)cap);

    return true;
}

Chili chiStringBuilderBuild(Chili b) {
    ChiStringBuilder* sb = chiToStringBuilder(b);

    Chili buf = chiFieldRead(&sb->buf);
    if (CHI_POISON_ENABLED)
        chiFieldWrite(CHI_CURRENT_PROCESSOR, b, &sb->buf, CHI_FALSE);

    uint32_t size = chiToUInt32(sb->size);
    uint8_t* bytes = chiToPreString(buf)->bytes;

    // Unboxed small strings
    if (chiFitsSmallString(size))
        return chiFromSmallString(bytes, size);

    size_t allocW = chiSize(buf), sizeW = CHI_BYTES_TO_WORDS(size + TRAILING_SIZE_BYTE);
    ChiGen gen = chiGen(buf);
    Chili str;
    if (gen == CHI_GEN_MAJOR) {
        if (sizeW < allocW)
            chiObjectSetSize(chiObject(buf), sizeW);
        str = chiWrapLarge(bytes, sizeW, CHI_STRING);
    } else {
        str = chiWrap(bytes, sizeW, CHI_STRING, gen);
    }

    setTrailSize(bytes, size);
    CHI_ASSERT(chiStringSize(str) == size);
    return str;
}

/**
 * Strings need special pinning treatment due to the unboxed small
 * string optimization. Chili strings are always zero terminated.
 * However it must still be ensured that the string does not contain inner zero bytes.
 */
Chili chiStringPin(Chili c) {
    if (!chiTrue(c) || (!chiUnboxed(c) && chiGen(c) == CHI_GEN_MAJOR))
        return c;

    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;

    if (CHI_STRING_UNBOXING && chiUnboxed63(c)) {
        const uint8_t *s = (const uint8_t*)&c, *src = s + CHI_SMALLSTRING_BYTES;
        uint32_t size = s[CHI_SMALLSTRING_SIZE];
        CHI_ASSERT(size <= CHI_SMALLSTRING_MAX);
        Chili d = newStringUninitialized(proc, size, CHI_NEW_SHARED | CHI_NEW_CLEAN);
        uint8_t* dst = (uint8_t*)chiRawPayload(d);
        for (uint32_t i = 0; i < CHI_SMALLSTRING_MAX; ++i) {
            if (size) {
                --size;
                dst[i] = src[i];
            }
        }
        return d;
    }

    chiPromoteLocal(proc, &c);
    return c;
}
