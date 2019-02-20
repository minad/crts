#include "location.h"
#include "stack.h"
#include "runtime.h"
#include "sink.h"
#include "num.h"
#include "ascii.h"

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
        *to++ = *from++;
        if (from[-1] == '_' && from + 1 < end) {
            to[-1] = (uint8_t)(chiToHexDigit((char)from[0]) * 16 + chiToHexDigit((char)from[1]));
            from += 2;
        }
    }
    return (uint32_t)(to - dst);
}

void chiLocLookupDefault(ChiLocLookup* lookup, ChiLoc loc) {
    CHI_ASSERT(loc.type == CHI_LOC_NATIVE);
    CHI_CLEAR(&lookup->loc);

    const ChiContInfo* info = chiContInfo(loc.native.cont);
    const char* locFile = CHI_AND(CHI_LOC_ENABLED, CHI_IFELSE(CHI_CONTINFO_PREFIX, info->loc->file, info->file));
    const char* locFn = CHI_AND(CHI_LOC_ENABLED, CHI_IFELSE(CHI_CONTINFO_PREFIX, info->loc->name, info->name));
    if (!locFile) {
        chiShowHexUInt(lookup->buf, (uintptr_t)loc.native.cont);
        lookup->loc.fn = chiStringRef(lookup->buf);
        lookup->loc.file = CHI_EMPTY_STRINGREF;
        lookup->loc.module = CHI_STRINGREF("Unknown");
        return;
    }

    ChiStringRef cFile = chiStringRef(locFile);
    ChiStringRef chiFile = chiStringRef(locFile + cFile.size + 1);

    const char* p = strrchr((const char*)cFile.bytes, '/');
    ChiStringRef module = p ? chiStringRef(p + 1) : cFile;
    module.size -= 2;

    bool mangled = CHI_CURRENT_RUNTIME->option.stack.traceMangled;
    lookup->loc.line = !mangled && chiFile.size ? info->line : info->lineMangled;
    lookup->loc.file = !mangled && chiFile.size ? chiFile : cFile;

    uint8_t* buf = (uint8_t*)lookup->buf, *end = buf + sizeof (lookup->buf);
    if (!mangled && buf + module.size <= end) {
        uint32_t n = demangle(buf, module.bytes, module.size);
        module.size = n;
        module.bytes = buf;
        buf += n;
    }
    lookup->loc.module = module;

    ChiStringRef fn = chiStringRef(locFn);
    if (!mangled && chiFile.size && buf + fn.size <= end) {
        uint32_t n = demangle(buf, fn.bytes, fn.size);
        fn.bytes = buf;
        fn.size = n;
    }
    lookup->loc.fn = fn;
}

ChiLoc chiLocateFrameDefault(const Chili* p) {
    Chili c = chiFrame(p) == CHI_FRAME_ENTER ? p[1 - chiToUnboxed(p[-1])] : *p;
    return (ChiLoc){ .native = { .cont = chiToCont(c), .type = CHI_LOC_NATIVE } };
}

ChiLoc chiLocateFnDefault(Chili c) {
    CHI_ASSERT(chiType(c) == CHI_THUNKFN || chiFn(chiType(c)));
    return (ChiLoc){ .native = { .cont = chiToCont(CHI_IDX(c, 0)), .type = CHI_LOC_NATIVE } };
}

CHI_ALIAS(chiLocLookupDefault) void chiLocLookup(ChiLocLookup*, ChiLoc);
CHI_ALIAS(chiLocateFrameDefault) ChiLoc chiLocateFrame(const Chili*);
CHI_ALIAS(chiLocateFnDefault) ChiLoc chiLocateFn(Chili);
