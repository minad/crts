#include "debug.h"
#include "error.h"
#include "mem.h"
#include "num.h"
#include "sink.h"
#include "strutil.h"

typedef _ChiSinkFile SinkFile;
typedef _ChiSinkProxy SinkProxy;

enum { SINKBUFSZ = 256 };

typedef struct {
    SinkProxy proxy;
    size_t    capacity, used;
    uint8_t   buf[0];
} SinkBuffer;

typedef struct {
    SinkProxy       proxy;
    ChiMutex        mutex;
    size_t          capacity;
    _Atomic(size_t) used;
    uint8_t         buf[0];
} SinkLock;

static void sinkCloseNoFlush(ChiSink* sink) {
    sink->close(sink);
}

static CHI_WU bool sinkTryFlush(ChiSink* sink) {
    return sink->flush(sink);
}

static CHI_WU bool sinkTryWrite(ChiSink* sink, const void* buf, size_t size) {
    return sink->write(sink, buf, size);
}

static CHI_WU bool sinkProxyWrite(ChiSink* sink, const void* buf, size_t size) {
    return sinkTryWrite(((SinkProxy*)sink)->sink, buf, size);
}

static CHI_WU bool sinkProxyFlush(ChiSink* sink) {
    return sinkTryFlush(((SinkProxy*)sink)->sink);
}

static void sinkProxyClose(ChiSink* sink) {
    sinkCloseNoFlush(((SinkProxy*)sink)->sink);
    chiFree(sink);
}

static CHI_WU bool sinkBufferFlush(ChiSink* sink) {
    SinkBuffer* s = (SinkBuffer*)sink;
    if (s->used) {
        if (!sinkProxyWrite(sink, s->buf, s->used))
            return false;
        s->used = 0;
    }
    return true;
}

static CHI_WU bool sinkBufferWrite(ChiSink* sink, const void* buf, size_t size) {
    SinkBuffer* s = (SinkBuffer*)sink;

    if (size >= s->capacity)
        return sinkBufferFlush(sink) && sinkProxyWrite(sink, buf, size);

    if (s->used + size > s->capacity && !sinkBufferFlush(sink))
        return false;

    memcpy(s->buf + s->used, buf, size);
    s->used += size;
    return true;
}

static CHI_WU bool sinkLockFlush(ChiSink* sink) {
    SinkLock* s = (SinkLock*)sink;
    CHI_LOCK_MUTEX(&s->mutex);
    size_t used = atomic_exchange_explicit(&s->used, s->capacity, memory_order_acq_rel);
    if (used && !sinkProxyWrite(sink, s->buf, used)) {
        atomic_store_explicit(&s->used, used, memory_order_release);
        return false;
    }
    atomic_store_explicit(&s->used, 0, memory_order_release);
    return true;
}

static CHI_WU bool sinkLockWrite(ChiSink* sink, const void* buf, size_t size) {
    SinkLock* s = (SinkLock*)sink;

    for (;;) {
        size_t oldUsed = atomic_load_explicit(&s->used, memory_order_acquire), newUsed = oldUsed + size;
        if (newUsed > s->capacity)
            break;
        if (atomic_compare_exchange_weak_explicit(&s->used, &oldUsed, newUsed,
                                                  memory_order_acq_rel,
                                                  memory_order_relaxed)) {
            memcpy(s->buf + oldUsed, buf, size);
            return true;
        }
    }

    CHI_LOCK_MUTEX(&s->mutex);
    size_t used = atomic_exchange_explicit(&s->used, s->capacity, memory_order_acq_rel);
    if (size >= s->capacity) {
        if (used && !sinkProxyWrite(sink, s->buf, used)) {
            atomic_store_explicit(&s->used, used, memory_order_release);
            return false;
        }
        if (!sinkProxyWrite(sink, buf, size))
            return false;
        atomic_store_explicit(&s->used, 0, memory_order_release);
    } else if (used + size > s->capacity) {
        if (used && !sinkProxyWrite(sink, s->buf, used)) {
            atomic_store_explicit(&s->used, used, memory_order_release);
            return false;
        }
        memcpy(s->buf, buf, size);
        atomic_store_explicit(&s->used, size, memory_order_release);
    } else {
        memcpy(s->buf + used, buf, size);
        atomic_store_explicit(&s->used, used + size, memory_order_release);
    }
    return true;
}

static void sinkLockClose(ChiSink* sink) {
    chiMutexDestroy(&((SinkLock*)sink)->mutex);
    sinkProxyClose(sink);
}

static CHI_WU bool sinkNullWrite(ChiSink* CHI_UNUSED(sink), const void* CHI_UNUSED(buf), size_t CHI_UNUSED(size)) {
    return true;
}

static CHI_WU bool sinkNullFlush(ChiSink* CHI_UNUSED(sink)) {
    return true;
}

static void sinkNullClose(ChiSink* CHI_UNUSED(sink)) {
}

static void sinkFree(ChiSink* sink) {
    chiFree(sink);
}

static CHI_WU bool sinkFileWrite(ChiSink* sink, const void* buf, size_t size) {
    return chiFileWrite(((SinkFile*)sink)->handle, buf, size);
}

static void sinkFileClose(ChiSink* sink) {
    chiFileClose(((SinkFile*)sink)->handle);
    chiFree(sink);
}

static ChiSink* sinkFileNew(ChiFile handle, void (*close)(ChiSink*)) {
    SinkFile* s = chiAllocObj(SinkFile);
    s->handle = handle;
    s->base.write = sinkFileWrite;
    s->base.flush = sinkNullFlush;
    s->base.close = close;
    return &s->base;
}

static CHI_WU bool sinkColorWrite(ChiSink* sink, const void* buf, size_t size) {
    char staticDst[SINKBUFSZ], *dst = size <= sizeof (staticDst) ? staticDst : (char*)chiAlloc(size), *t = dst;
    const char* q = (const char*)buf, *end = q + size;
    while (q < end) {
        char* p = (char*)memccpy(t, q, '\033', (size_t)(end - q));
        if (!p) {
            t += (size_t)(end - q);
            break;
        }

        q += (size_t)(p - t);
        t = p - 1;

        if (q < end && *q == '[') { // ansi sequence
            ++q;
            while (q < end) {
                char c = *q++;
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) // end of ansi sequence
                    break;
            }
        }
    }
    bool res = sinkTryWrite(((SinkProxy*)sink)->sink, dst, (size_t)(t - dst));
    if (dst != staticDst)
        chiFree(dst);
    return res;
}

static ChiSink* sinkColorNew(ChiSink* sink, ChiFile handle, ChiSinkColor color) {
    if (!CHI_COLOR_ENABLED)
        return sink;
    if (color == CHI_SINK_COLOR_AUTO)
        color = chiFileTerminal(handle) ? CHI_SINK_COLOR_ON : CHI_SINK_COLOR_OFF;
    if (color != CHI_SINK_COLOR_OFF)
        return sink;
    SinkProxy* s = chiAllocObj(SinkProxy);
    s->sink = sink;
    s->base.write = sinkColorWrite;
    s->base.close = sinkProxyClose;
    s->base.flush = sinkProxyFlush;
    return &s->base;
}

static ChiSink* sinkLockNew(ChiSink* sink, size_t cap) {
    SinkLock* s = (SinkLock*)chiAlloc(sizeof (SinkLock) + cap);
    chiMutexInit(&s->mutex);
    s->proxy.sink = sink;
    s->proxy.base.write = sinkLockWrite;
    s->proxy.base.close = sinkLockClose;
    s->proxy.base.flush = sinkLockFlush;
    atomic_store_explicit(&s->used, 0, memory_order_relaxed);
    s->capacity = cap;
    return &s->proxy.base;
}

static ChiSink* sinkBufferNew(ChiSink* sink, size_t cap) {
    SinkBuffer* s = (SinkBuffer*)chiAlloc(sizeof (SinkBuffer) + cap);
    s->proxy.sink = sink;
    s->proxy.base.write = sinkBufferWrite;
    s->proxy.base.close = sinkProxyClose;
    s->proxy.base.flush = sinkBufferFlush;
    s->used = 0;
    s->capacity = cap;
    return &s->proxy.base;
}

ChiSink* chiSinkFileTryNew(const char* file, size_t buffer, bool locked, ChiSinkColor color) {
    ChiFile handle;
    bool close = true;

    if (streq(file, "null"))
        return chiSinkNull;

    if (streq(file, "stdout")) {
        handle = CHI_FILE_STDOUT;
        close = false;
    } else if (streq(file, "stderr")) {
        handle = CHI_FILE_STDERR;
        close = false;
    } else if (strstarts(file, "fd:") && chiDigit(file[3]) && !file[4]) {
        handle = chiFileOpenFd(file[3] - '0');
    } else {
        handle = chiFileOpen(file);
    }

    if (chiFileNull(handle)) {
        chiWarn("Could not open file '%s' for writing: %m", file);
        return 0;
    }

    if (chiFileTerminal(handle) && color == CHI_SINK_BINARY) {
        chiWarn("Refusing to write binary data to '%s'", file);
        if (close)
            chiFileClose(handle);
        return 0;
    }

    ChiSink* sink = sinkFileNew(handle, close ? sinkFileClose : sinkFree);
    sink = locked ? sinkLockNew(sink, buffer) : sinkBufferNew(sink, buffer);
    return sinkColorNew(sink, handle, color);
}

ChiSink* chiSinkColor(ChiSinkColor color) {
    return sinkColorNew(&_chiStdout.base, CHI_FILE_STDOUT, color);
}

static CHI_WU bool sinkStdoutInit(ChiSink* sink, const void* buf, size_t size) {
    SinkProxy* s = (SinkProxy*)sink;
    s->sink = sinkLockNew(sinkFileNew(CHI_FILE_STDOUT, sinkFree), CHI_KiB(8));
    s->base.flush = sinkProxyFlush;
    s->base.write = sinkProxyWrite;
    return sinkProxyWrite(sink, buf, size);
}

static CHI_WU bool sinkStderrWrite(ChiSink* sink, const void* buf, size_t size) {
    CHI_IGNORE_RESULT(chiFileWrite(((SinkFile*)sink)->handle, buf, size));
    return true;
}

static CHI_WU bool sinkStderrInit(ChiSink* sink, const void* buf, size_t size) {
    ((SinkFile*)sink)->handle = CHI_FILE_STDERR;
    sink->write = sinkStderrWrite;
    return sinkStderrWrite(sink, buf, size);
}

CHI_INTERN const ChiSink _chiSinkNull =
    { .write = sinkNullWrite,
      .flush = sinkNullFlush,
      .close = sinkNullClose };
CHI_INTERN SinkProxy _chiStdout =
    { .base = { .write = sinkStdoutInit,
                .flush = sinkNullFlush,
                .close = sinkNullClose } };
CHI_INTERN SinkFile _chiStderr =
    { .base = { .write = sinkStderrInit,
                .flush = sinkNullFlush,
                .close = sinkNullClose }, };

CHI_DESTRUCTOR(sink) {
    if (_chiStdout.sink) {
        chiSinkClose(&_chiStdout.base);
        CHI_POISON_STRUCT(&_chiStdout, CHI_POISON_DESTROYED);
    }
}

#define VEC_NOSTRUCT
#define VEC ChiByteVec
#define VEC_TYPE uint8_t
#define VEC_PREFIX byteVec
#include "generic/vec.h"

static CHI_WU bool sinkStringWrite(ChiSink* sink, const void* buf, size_t size) {
    byteVecAppendMany(&((ChiSinkString*)sink)->vec, (const uint8_t*)buf, size);
    return true;
}

static void sinkStringClose(ChiSink* sink) {
    byteVecFree(&((ChiSinkString*)sink)->vec);
}

static void sinkStringFree(ChiSink* sink) {
    sinkStringClose(sink);
    chiFree(sink);
}

ChiStringRef chiSinkString(ChiSink* sink) {
    ChiSinkString* s = (ChiSinkString*)sink;
    CHI_ASSERT(s->base.close == sinkStringClose || sink->close == sinkStringFree);
    ChiStringRef ret = { .size = (uint32_t)s->vec.used, .bytes = s->vec.elem };
    s->vec.used = 0;
    return ret;
}

const char* chiSinkCString(ChiSink* sink) {
    chiSinkPutc(sink, '\0');
    return (const char*)chiSinkString(sink).bytes;
}

ChiSink* chiSinkStringInit(ChiSinkString* s) {
    CHI_ZERO_STRUCT(&s->vec);
    s->base.write = sinkStringWrite;
    s->base.close = sinkStringClose;
    s->base.flush = sinkNullFlush;
    return &s->base;
}

ChiSink* chiSinkStringNew(void) {
    ChiSinkString* s = chiAllocObj(ChiSinkString);
    chiSinkStringInit(s);
    s->base.close = sinkStringFree;
    return &s->base;
}

static CHI_WU bool sinkMemWrite(ChiSink* sink, const void* buf, size_t size) {
    ChiSinkMem* s = (ChiSinkMem*)sink;
    size_t n = s->used + size < s->capacity ? size : (size_t)(s->capacity - s->used);
    memcpy(s->buf + s->used, buf, n);
    s->used += n;
    return true;
}

ChiSink* chiSinkMemInit(ChiSinkMem* s, void* buf, size_t cap) {
    s->base.write = sinkMemWrite;
    s->base.close = sinkNullClose;
    s->base.flush = sinkNullFlush;
    s->buf = (char*)buf;
    s->used = 0;
    s->capacity = cap;
    return &s->base;
}

void chiSinkFlush(ChiSink* sink) {
    if (!sinkTryFlush(sink))
        chiErr("Failed to flush sink: %m");
}

void chiSinkWrite(ChiSink* sink, const void* buf, size_t size) {
    if (!sinkTryWrite(sink, buf, size))
        chiErr("Failed to write to sink: %m");
}

void chiSinkClose(ChiSink* sink) {
    CHI_IGNORE_RESULT(sinkTryFlush(sink));
    sinkCloseNoFlush(sink);
}

/**
 * Write quoted string using JSON-like escaping.
 * Control characters are escaped. UTF-8 characters are not escaped.
 */
size_t chiSinkWriteq(ChiSink* sink, const uint8_t* bytes, size_t size) {
    char out[SINKBUFSZ] = { '"' };
    size_t n = 1, written = 0;
    for (size_t i = 0; i < size; ++i) {
        if (n + 5 > sizeof (out)) {
            chiSinkWrite(sink, out, n);
            written += n;
            n = 0;
        }
        uint8_t c = bytes[i];
        switch (c) {
        case '"':
            out[n++] = '\\';
            out[n++] = '"';
            break;
        case '\n':
            out[n++] = '\\';
            out[n++] = 'n';
            break;
        case '\\':
            out[n++] = '\\';
            out[n++] = '\\';
            break;
        default:
            if (c > 0x1F) {
                out[n++] = (char)c;
            } else {
                out[n++] = '\\';
                out[n++] = 'x';
                out[n++] = chiHexDigits[(c / 16) & 15];
                out[n++] = chiHexDigits[c & 15];
            }
        }
    }
    out[n++] = '"';
    chiSinkWrite(sink, out, n);
    return written + n;
}

size_t chiSinkWriteh(ChiSink* sink, const void* buf, size_t size) {
    char out[SINKBUFSZ] = { '"' };
    size_t n = 1;
    for (const uint8_t *p = (const uint8_t*)buf; p < (const uint8_t*)buf + size; ++p) {
        if (n + 3 > sizeof (out)) {
            chiSinkWrite(sink, out, n);
            n = 0;
        }
        out[n++] = chiHexDigits[(*p / 16) & 15];
        out[n++] = chiHexDigits[*p & 15];
    }
    out[n++] = '"';
    chiSinkWrite(sink, out, n);
    return 2 * size + 2;
}

size_t chiSinkPutu(ChiSink* sink, uint64_t x) {
    char buf[CHI_INT_BUFSIZE];
    size_t n = (size_t)(chiShowUInt(buf, x) - buf);
    chiSinkWrite(sink, buf, n);
    return n;
}

void chiSinkWarnv(ChiSink* sink, const char* fmt, va_list ap) {
    char buf[SINKBUFSZ];
    ChiSinkMem s;
    chiSinkMemInit(&s, buf, sizeof (buf));
    chiSinkPuts(&s.base, "Error: ");
    chiSinkFmtv(&s.base, fmt, ap);
    s.used = CHI_MIN(s.capacity - 1, s.used);
    chiSinkPutc(&s.base, '\n');
    chiSinkWrite(sink, s.buf, s.used);
}

void chiSinkWarn(ChiSink* sink, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkWarnv(sink, fmt, ap);
    va_end(ap);
}
