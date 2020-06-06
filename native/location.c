#include "location.h"
#include "runtime.h"
#include "sink.h"
#include "stack.h"

static uint32_t demangle(uint8_t* dst, const uint8_t* src, uint32_t size) {
    const uint8_t* end = src + size, *from = src;
    while (from < end && *from != '_')
        ++from;
    ++from;
    if (from >= end) {
        memcpy(dst, src, size);
        return size;
    }
    uint8_t* to = dst;
    while (from < end) {
        if (from[0] == '_' && from + 2 < end && from[1] == '_' && from[2] == '_') {
            *to++ = '.';
            from += 3;
        } else if (from[0] == '_' && from + 1 < end && from[1] == '_') {
            *to++ = '/';
            from += 2;
        } else if (from[0] == '_' && from + 2 < end && chiHexDigit((char)from[1]) && chiHexDigit((char)from[2])) {
            *to++ = (uint8_t)(chiToHexDigit((char)from[1]) * 16 + chiToHexDigit((char)from[2]));
            from += 3;
        } else {
            *to++ = *from++;
        }
    }
    return (uint32_t)(to - dst);
}

void chiLocResolveDefault(ChiLocResolve* resolve, ChiLoc loc, bool mangled) {
    CHI_ASSERT(loc.type == CHI_LOC_NATIVE);
    CHI_ZERO_STRUCT(&resolve->loc);

    ChiContInfo* info = chiContInfo(loc.cont);
    CHI_NOWARN_UNUSED(info);

    const char* file = CHI_AND(CHI_LOC_ENABLED, CHI_CHOICE(CHI_CONT_PREFIX, info->loc->file, info->file));
    ChiStringRef fn;
    if (!file) {
        fn.bytes = (uint8_t*)resolve->buf;
        fn.size = (uint32_t)(chiShowHexUInt(resolve->buf, (uintptr_t)loc.cont) - resolve->buf);
        resolve->loc.file = CHI_EMPTY_STRINGREF;
    } else {
        resolve->loc.line = CHI_AND(CHI_LOC_ENABLED, CHI_CHOICE(CHI_CONT_PREFIX, info->loc->line, info->line));
        ChiStringRef ext = resolve->loc.file = chiStringRef(file);

        fn = chiStringRef((const char*)CHI_AND(CHI_LOC_ENABLED, CHI_CHOICE(CHI_CONT_PREFIX, info->loc->name, info->name)));
        if (!mangled
            && ext.size && ext.bytes[ext.size - 1] != 'c'
            && fn.size <= sizeof (resolve->buf)) {
            uint32_t n = demangle((uint8_t*)resolve->buf, fn.bytes, fn.size);
            fn.bytes = (uint8_t*)resolve->buf;
            fn.size = n;
        }
    }
    resolve->loc.fn = fn;
}

ChiLoc chiLocateFrameDefault(const Chili* p) {
    Chili c = chiToCont(*p) == &chiRestoreCont ? p[1 - chiToUnboxed(p[-1])] : *p;
    return (ChiLoc){ .cont = chiToCont(c), .type = CHI_LOC_NATIVE };
}
