#include <math.h>
#include "error.h"
#include "location.h"
#include "new.h"
#include "runtime.h"
#include "sink.h"

enum { FMTBUFSZ = 256 };

size_t chiLocFmt(ChiSink* sink, const ChiLocInfo* loc, ChiLocFmt what) {
    size_t written = 0;
    if (what & CHI_LOCFMT_FN)
        written += chiSinkPuts(sink, loc->fn);
    if ((what & CHI_LOCFMT_INTERP) && loc->interp)
        written += chiSinkPuts(sink, "[i]");
    if ((what & CHI_LOCFMT_FILE) && loc->file.size)
        written += chiSinkFmt(sink, " (%S:%u)", loc->file, loc->line);
    if ((what & CHI_LOCFMT_FILESEP) && loc->file.size)
        written += chiSinkFmt(sink, "\n%S:%u", loc->file, loc->line);
    return written;
}

static void fmtDouble(ChiStringRef* field, char* buf, uint32_t prec, char fmt, double x) {
    if (!prec)
        prec = 6;
    double a = x < 0 ? -x : x;
    if (__builtin_isnan(x)) {
        *field = CHI_STRINGREF("NaN");
    } else if (__builtin_isinf(x)) {
        *field = CHI_STRINGREF("-Inf");
        if (x > 0) {
            ++field->bytes;
            --field->size;
        }
    } else if (a < 1e-99 || (a >= 1 && a < 10) || (fmt == 'f' && a < 1e12)) {
        // Precise fix point formatting for small doubles (Locale independent!)
        CHI_SETMIN(&prec, 19);
        int64_t s = (int64_t)a;
        uint64_t r = chiPow10[prec], f = (uint64_t)((a - (double)s) * (double)r + 0.5);
        if (f >= r) {
            f -= r;
            ++s;
        }
        field->size = (uint32_t)chiFmt(buf, FMTBUFSZ, "%jd.%0*ju", x < 0 ? -s : s, prec, f);
    } else {
        // Imprecise formatting in scientific notation.
        double e = floor(log10(a));
        field->size = (uint32_t)chiFmt(buf, FMTBUFSZ, "%.*fe%d", prec,
                                       x / pow(10, e), (int32_t)e);
    }
}

static void fmtNanos(ChiStringRef* field, char* buf, uint32_t prec, uint32_t* width, uint64_t t) {
    if (!prec)
        prec = 2;
    if (!t) {
        *field = CHI_STRINGREF("0s");
    } else {
        field->size = (uint32_t)
            (t >= 1000000000 ? chiFmt(buf, FMTBUFSZ, "%.*fs", prec, (double)t / 1e9) :
             t >= 1000000 ? chiFmt(buf, FMTBUFSZ, "%.*fms", prec, (double)t / 1e6) :
             t >= 1000 ? chiFmt(buf, FMTBUFSZ, "%.*fµs", prec, (double)t / 1e3) :
             chiFmt(buf, FMTBUFSZ, "%juns", t));
        if (t < 1000000 && t >= 1000 && *width)
            ++(*width);
    }
}

static uint32_t fmtChili(char* buf, Chili c) {
    if (!chiRef(c))
        return (uint32_t)chiFmt(buf, FMTBUFSZ, "U%jx", chiToUnboxed(c));
    ChiGen gen = chiGen(c);
    ChiType type = chiType(c);
    char tag[16] = "";
    if (type <= CHI_LAST_TAG)
        chiFmt(tag, sizeof (tag), "tag=%u,", (uint32_t)type);
    else if (chiFn(type))
        chiFmt(tag, sizeof (tag), "arity=%u,", chiFnTypeArity(type));
    if (gen == CHI_GEN_MAJOR) {
        ChiObject* obj = chiObjectUnchecked(c);
        return (uint32_t)chiFmt(buf, FMTBUFSZ,
                                "%s[addr=%lx,size=%zu,%s%s,color=%u" CHI_IF(CHI_POISON_ENABLED, ",owner=%u") "%s%s]",
                                chiTypeName(type), chiAddress(c), obj->size, tag,
                                chiObjectShared(obj) ? "major-shared" : "major-local",
                                CHI_UN(Color, chiObjectColor(obj)),
                                CHI_IF(CHI_POISON_ENABLED, chiObjectOwner(obj) - 1,)
                                obj->flags.dirty ? ",dirty" : "",
                                chiObjectLocked(obj) ? ",locked" : "");
    }
    return (uint32_t)chiFmt(buf, FMTBUFSZ,
                            "%s[addr=%lx,size=%zu,%sgen=%u]",
                            chiTypeName(type), chiAddress(c),
                            chiSizeSmall(c), tag, gen);
}

static uint32_t fmtSize(char* buf, size_t size) {
    size_t s = size;
    int i = 0;
    while (s >= 1024 && s % 1024 == 0 && i < 4) {
        s /= 1024;
        ++i;
    }
    char* end = chiShowUInt(buf, i >= 1 && i <= 4 ? s : size);
    if (i >= 1 && i <= 4)
        *end++ = "KMGT"[i - 1];
    return (uint32_t)(end - buf);
}

static void fmtPtr(ChiStringRef* field, char* buf, uintptr_t p) {
    if (p) {
        buf[0] = '0';
        buf[1] = 'x';
        field->size = (uint32_t)(chiShowHexUInt(buf + 2, p) - buf);
    } else {
        *field = CHI_STRINGREF("(null)");
    }
}

size_t chiSinkFmtv(ChiSink* sink, const char* fmt, va_list ap) {
    size_t written = 0;
    for (const char* p = fmt; *p; ++p) {
        if (*p != '%') {
            const char* start = p;
            do {
                p++;
            } while (*p && *p != '%');
            size_t n = (size_t)(p - start);
            chiSinkWrite(sink, start, n);
            written += n;
            if (!*p)
                break;
        }
        ++p;

        bool left = *p == '-' && *p++;
        char pad = *p == '0' ? *p++ : ' ';

        uint32_t width = 0;
        if (*p == '*') {
            width = va_arg(ap, uint32_t);
            ++p;
        } else {
            CHI_IGNORE_RESULT(chiReadUInt32(&width, &p));
        }

        uint32_t prec = 0;
        if (*p == '.') {
            ++p;
            if (*p == '*') {
                prec = va_arg(ap, uint32_t);
                ++p;
            } else {
                CHI_IGNORE_RESULT(chiReadUInt32(&prec, &p));
            }
        }

        char quote = *p == 'q' || *p == 'h' ? *p++ : 0;
        if (!*p)
            break;

        char size = *p == 'l' || *p == 'j' || *p == 'z' ? *p++ : 0;
        if (!*p)
            break;

        char fmtBuf[FMTBUFSZ];
        ChiStringRef field = { .bytes = (uint8_t*)fmtBuf };
        switch (*p) {
        case 'L':
            written += chiLocFmt(sink, va_arg(ap, const ChiLocInfo*), width ? (ChiLocFmt)width : CHI_LOCFMT_ALL);
            continue;
        case '%':
            fmtBuf[0] = '%';
            field.size = 1;
            break;
        case 'c': // char
            fmtBuf[0] = (char)va_arg(ap, int);
            field.size = 1;
            break;
        case 'w': // whitespace, separator
            field.size = 0;
            break;
        case 'S': // ChiStringRef
#ifdef CHI_STANDALONE_WASM
            field = *va_arg(ap, ChiStringRef*);
#else
            field = va_arg(ap, ChiStringRef);
#endif
            break;
        case 'b':
            field.size = va_arg(ap, uint32_t);
            field.bytes = va_arg(ap, const uint8_t*);
            break;
        case 's': // const char*
            {
                const char* s = va_arg(ap, const char*);
                field = s ? chiStringRef(s) : CHI_STRINGREF("(null)");
                break;
            }
        case 't': // ChiNanos
            fmtNanos(&field, fmtBuf, prec, &width, CHI_UN(Nanos, va_arg(ap, ChiNanos)));
            break;
        case 'C': // Chili
            field.size = fmtChili(fmtBuf, va_arg(ap, Chili));
            break;
        case 'Z': // Size in K, M, G, T
            field.size = fmtSize(fmtBuf, va_arg(ap, size_t));
            break;
        case 'd': // int
            {
                int64_t x =
                    size == 'z' ? (sizeof (size_t) == 8 ? (int64_t)va_arg(ap, size_t) : (int32_t)va_arg(ap, size_t)) :
                    size == 'j' || (size == 'l' && sizeof (long) == 8) ? va_arg(ap, int64_t) : va_arg(ap, int32_t);
                field.size = (uint32_t)(chiShowInt(fmtBuf, x) - fmtBuf);
                break;
            }
        case 'x': // unsigned int
        case 'u': // unsigned int
            {
                uint64_t x =
                    size == 'z' ? (sizeof (size_t) == 8 ? (uint64_t)va_arg(ap, size_t) : (uint32_t)va_arg(ap, size_t)) :
                    size == 'j' || (size == 'l' && sizeof (long) == 8) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                char* end = *p == 'u' ? chiShowUInt(fmtBuf, x) : chiShowHexUInt(fmtBuf, x);
                field.size = (uint32_t)(end - fmtBuf);
                break;
            }
        case 'p': // void*
            fmtPtr(&field, fmtBuf, va_arg(ap, uintptr_t));
            break;
        case 'f': // double
        case 'e': // double
            fmtDouble(&field, fmtBuf, prec, *p, va_arg(ap, double));
            break;
        case 'm': // errno
            field = CHI_AND(CHI_SYSTEM_HAS_ERRNO, chiErrnoString(fmtBuf, FMTBUFSZ)) ? chiStringRef(fmtBuf) : CHI_STRINGREF("Unknown error");
            break;
        default:
            CHI_BUG("Invalid format specifier");
        }

        if (!quote) {
            for (uint32_t i = field.size; !left && i < width; ++i)
                chiSinkPutc(sink, pad);
            chiSinkPuts(sink, field);
            for (uint32_t i = field.size; left && i < width; ++i)
                chiSinkPutc(sink, ' ');
            written += CHI_MAX(field.size, width);
        } else if (quote == 'q') {
            written += chiSinkPutq(sink, field);
        } else {
            CHI_ASSERT(quote == 'h');
            written += chiSinkPuth(sink, field);
        }
    }
    return written;
}

size_t chiSinkFmt(ChiSink* sink, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t written = chiSinkFmtv(sink, fmt, ap);
    va_end(ap);
    return written;
}

size_t chiFmtv(char* buf, size_t bufsize, const char* fmt, va_list ap) {
    ChiSinkMem s;
    chiSinkMemInit(&s, buf, bufsize);
    size_t n = chiSinkFmtv(&s.base, fmt, ap);
    if (n >= bufsize) {
        buf[bufsize - 1] = 0;
        chiErr("Formatted string truncated '%s'.\n", buf);
    }
    buf[n] = 0;
    return n;
}

size_t chiFmt(char* buf, size_t bufsize, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t ret = chiFmtv(buf, bufsize, fmt, ap);
    va_end(ap);
    return ret;
}

Chili chiFmtString(ChiProcessor* proc, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Chili ret = chiFmtStringv(proc, fmt, ap);
    va_end(ap);
    return ret;
}

Chili chiFmtStringv(ChiProcessor* proc, const char* fmt, va_list ap) {
    CHI_STRING_SINK(sink);
    chiSinkFmtv(sink, fmt, ap);
    return chiStringNew(proc, chiSinkString(sink));
}
