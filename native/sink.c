#include "sink.h"
#include "num.h"
#include "mem.h"
#include "strutil.h"
#include "system.h"
#include "error.h"
#include "ascii.h"
#include "system.h"

typedef struct {
    ChiSink  base;
    ChiSink* sink;
    size_t   capacity, used;
    uint8_t  buf[0];
} SinkBuffer;

typedef struct {
    ChiSink         base;
    ChiSink*        sink;
    ChiRWLock       lock;
    size_t          capacity;
    _Atomic(size_t) used;
    uint8_t         buf[0];
} SinkLock;

typedef struct {
    ChiSink  base;
    ChiSink* sink;
} SinkColor;

static void sinkBufferFlush(ChiSink* sink) {
    SinkBuffer* s = (SinkBuffer*)sink;
    if (s->used) {
        chiSinkWrite(s->sink, s->buf, s->used);
        s->used = 0;
    }
}

static void sinkBufferWrite(ChiSink* sink, const void* buf, size_t size) {
    SinkBuffer* s = (SinkBuffer*)sink;

    if (CHI_UNLIKELY(size >= s->capacity)) {
        sinkBufferFlush(sink);
        chiSinkWrite(s->sink, buf, size);
        return;
    }

    if (s->used + size > s->capacity)
        sinkBufferFlush(sink);

    memcpy(s->buf + s->used, buf, size);
    s->used += size;
}

static void sinkBufferClose(ChiSink* sink) {
    SinkBuffer* s = (SinkBuffer*)sink;
    sinkBufferFlush(sink);
    chiSinkClose(s->sink);
    chiFree(sink);
}

static void _sinkLockFlush(SinkLock* s) {
    if (s->used) {
        chiSinkWrite(s->sink, s->buf, s->used);
        s->used = 0;
    }
}

static void sinkLockFlush(ChiSink* sink) {
    SinkLock* s = (SinkLock*)sink;
    CHI_LOCK_WRITE(&s->lock);
    _sinkLockFlush(s);
}

static void sinkLockWrite(ChiSink* sink, const void* buf, size_t size) {
    SinkLock* s = (SinkLock*)sink;

    if (CHI_UNLIKELY(size >= s->capacity)) {
        CHI_LOCK_WRITE(&s->lock);
        _sinkLockFlush(s);
        chiSinkWrite(s->sink, buf, size);
        return;
    }

    {
        CHI_LOCK_READ(&s->lock);
        for (;;) {
            size_t oldUsed = s->used, newUsed = oldUsed + size;
            if (newUsed > s->capacity)
                break;
            if (atomic_compare_exchange_weak(&s->used, &oldUsed, newUsed)) {
                memcpy(s->buf + oldUsed, buf, size);
                return;
            }
        }
    }

    CHI_LOCK_WRITE(&s->lock);
    if (s->used + size > s->capacity)
        _sinkLockFlush(s);

    memcpy(s->buf + s->used, buf, size);
    s->used += size;
}

static void sinkLockClose(ChiSink* sink) {
    SinkLock* s = (SinkLock*)sink;
    sinkLockFlush(sink);
    chiRWLockDestroy(&s->lock);
    chiSinkClose(s->sink);
    chiFree(sink);
}

static void sinkNullWrite(ChiSink* CHI_UNUSED(sink), const void* CHI_UNUSED(buf), size_t CHI_UNUSED(size)) {
}

static void sinkNullNop(ChiSink* CHI_UNUSED(sink)) {
}

static void sinkFree(ChiSink* sink) {
    chiFree(sink);
}

#if CHI_SYSTEM_HAS_STDIO
typedef struct {
    ChiSink base;
    FILE*   fp;
} SinkFile;

static void sinkFileWrite(ChiSink* sink, const void* buf, size_t size) {
    if (fwrite(buf, 1, size, ((SinkFile*)sink)->fp) != size)
        chiSysErr("fwrite");
}

static void sinkFileFlush(ChiSink* sink) {
    if (fflush(((SinkFile*)sink)->fp))
        chiSysErr("fflush");
}

static void sinkFileClose(ChiSink* sink) {
    fclose(((SinkFile*)sink)->fp);
    chiFree(sink);
}

static void sinkPipeClose(ChiSink* sink) {
    pclose(((SinkFile*)sink)->fp);
    chiFree(sink);
}

static void sinkFileUncheckedFlush(ChiSink* sink) {
    fflush(((SinkFile*)sink)->fp);
}

static void sinkFileUncheckedWrite(ChiSink* sink, const void* buf, size_t size) {
    fwrite(buf, 1, size, ((SinkFile*)sink)->fp);
}

static void sinkColorWrite(ChiSink* sink, const void* buf, size_t size) {
    ChiSink* out = ((SinkColor*)sink)->sink;
    const char* q = (const char*)buf, *end = q + size;
    while (q < end) {
        const char* p = (const char*)memchr(q, '\e', (size_t)(end - q));
        if (!p) {
            chiSinkWrite(out, q, (size_t)(end - q));
            break;
        }

        chiSinkWrite(out, q, (size_t)(p - q));
        q = p + 1;

        if (q < end && *q == '[') { // ansi sequence
            ++q;
            while (q < end) {
                char c = *q++;
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) // end of ansi sequence
                    break;
            }
        }
    }
}

static void sinkColorClose(ChiSink* sink) {
    chiSinkClose(((SinkColor*)sink)->sink);
    chiFree(sink);
}

static void sinkColorFlush(ChiSink* sink) {
    chiSinkFlush(((SinkColor*)sink)->sink);
}

static ChiSink* sinkColorNew(ChiSink* sink, int fd, ChiSinkColor color) {
    if (!CHI_COLOR_ENABLED)
        return sink;
    if (color == CHI_SINK_COLOR_AUTO)
        color = chiTerminal(fd) ? CHI_SINK_COLOR_ON : CHI_SINK_COLOR_OFF;
    if (color != CHI_SINK_COLOR_OFF)
        return sink;

    SinkColor* s = chiAllocObj(SinkColor);
    s->sink = sink;
    s->base.write = sinkColorWrite;
    s->base.close = sinkColorClose;
    s->base.flush = sinkColorFlush;
    return &s->base;
}

ChiSink* chiStderrColor(ChiSinkColor color) {
    return sinkColorNew(chiStderr, STDERR_FILENO, color);
}

ChiSink* chiStdoutColor(ChiSinkColor color) {
    return sinkColorNew(chiStdout, STDOUT_FILENO, color);
}

const char* chiZstdPath(void) {
    static const char* zstdPath = (const char*)-1;
    if (zstdPath == (const char*)-1) {
        const char* path = chiGetEnv("CHI_ZSTD");
        if (!path)
            path = CHI_ZSTD;
        zstdPath = chiFilePerm(path, CHI_FILE_EXECUTABLE) ? path : 0;
    }
    return zstdPath;
}

static ChiSink* sinkFileTryNew(const char* file, ChiSinkColor color) {
    FILE* fp = 0;
    void (*close)(ChiSink*) = sinkFileClose;

    if (CHI_SINK_FD_ENABLED && strstarts(file, "fd:") && chiDigit(file[3]) && !file[4]) {
        fp = chiOpenFd(file[3] - '0');
    } else if (CHI_SINK_ZSTD_ENABLED && strends(file, ".zst") && chiZstdPath()) {
        CHI_STRING_SINK(cmd);
        chiSinkFmt(cmd, "%s -q -1 -o %qs", chiZstdPath(), file);
        fp = chiOpenPipe(chiSinkCString(cmd));
        close = sinkPipeClose;
    } else {
        fp = chiFileOpenWrite(file);
    }
    if (!fp) {
        chiWarn("Could not open file '%s' for writing: %m", file);
        return 0;
    }

    SinkFile* s = chiAllocObj(SinkFile);
    s->fp = fp;
    s->base.write = sinkFileWrite;
    s->base.flush = sinkFileFlush;
    s->base.close = close;
    return sinkColorNew(&s->base, fileno(fp), color);
}

static void lazySinkInit(SinkFile* sink, FILE* fp) {
    sink->fp = fp;
    sink->base.flush = sinkFileUncheckedFlush;
    sink->base.write = sinkFileUncheckedWrite;
}

#define LAZY_SINK(Name, name)                                           \
    static void name##_sinkFlush(ChiSink* sink) {                       \
        lazySinkInit((SinkFile*)sink, name);                            \
        chiSinkFlush(sink);                                             \
    }                                                                   \
    static void name##_sinkWrite(ChiSink* sink, const void* buf, size_t size) { \
        lazySinkInit((SinkFile*)sink, name);                            \
        chiSinkWrite(sink, buf, size);                                  \
    }                                                                   \
    static SinkFile name##_sink =                                       \
        { .base = { .write = name##_sinkWrite,                          \
                    .flush = name##_sinkFlush,                          \
                    .close = sinkNullNop },                             \
        };                                                              \
    ChiSink *const Name = &name##_sink.base;

LAZY_SINK(chiStderr, stderr)
LAZY_SINK(chiStdout, stdout)
#elif defined(CHI_STANDALONE_SANDBOX)
typedef struct {
    ChiSink  base;
    uint32_t stream;
} SinkStream;

static void sinkStreamWrite(ChiSink* sink, const void* buf, size_t size) {
    sb_stream_write_all(((SinkStream*)sink)->stream, buf, size);
}

static SinkStream sinkOut =
    { .base = { .write = sinkStreamWrite,
                .flush = sinkNullNop,
                .close = sinkNullNop },
      .stream = SB_STREAM_OUT,
    };

static SinkStream sinkErr =
    { .base = { .write = sinkStreamWrite,
                .flush = sinkNullNop,
                .close = sinkNullNop },
      .stream = SB_STREAM_ERR,
    };

static ChiSink* sinkFileTryNew(const char* file, ChiSinkColor CHI_UNUSED(color)) {
    if (chiDigit(file[0]) && !file[1]) {
        uint32_t id = (uint32_t)(file[0] - '0');
        if (id < sb_info->stream_count && (sb_info->stream[id].mode & SB_MODE_WRITE)) {
            SinkStream* s = chiAllocObj(SinkStream);
            s->stream = id;
            s->base.write = sinkStreamWrite;
            s->base.flush = sinkNullNop;
            s->base.close = sinkFree;
            return &s->base;
        }
    }
    chiWarn("Could not open file '%s' for writing", file);
    return 0;
}

ChiSink* chiStderrColor(ChiSinkColor CHI_UNUSED(color)) {
    return chiStderr;
}

ChiSink* chiStdoutColor(ChiSinkColor CHI_UNUSED(color)) {
    return chiStdout;
}

ChiSink *const chiStdout = &sinkOut.base, *const chiStderr = &sinkErr.base;
#elif defined(CHI_STANDALONE_WASM)
typedef struct {
    ChiSink  base;
    uint32_t fd;
} SinkWasm;

static void sinkWasmWrite(ChiSink* sink, const void* buf, size_t size) {
    wasm_sink_write_all(((SinkWasm*)sink)->fd, buf, size);
}

static void sinkWasmClose(ChiSink* sink) {
    wasm_sink_close(((SinkWasm*)sink)->fd);
    chiFree(sink);
}

static SinkWasm sinkOut =
    { .base = { .write = sinkWasmWrite,
                .flush = sinkNullNop,
                .close = sinkNullNop },
      .fd = 1,
    };

static SinkWasm sinkErr =
    { .base = { .write = sinkWasmWrite,
                .flush = sinkNullNop,
                .close = sinkNullNop },
      .fd = 2,
    };

static ChiSink* sinkFileTryNew(const char* file, ChiSinkColor CHI_UNUSED(color)) {
    uint32_t fd = wasm_sink_open(file);
    if (fd == (uint32_t)-1) {
        chiWarn("Could not open file '%s' for writing", file);
        return 0;
    }
    SinkWasm* s = chiAllocObj(SinkWasm);
    s->fd = fd;
    s->base.write = sinkWasmWrite;
    s->base.flush = sinkNullNop;
    s->base.close = sinkWasmClose;
    return &s->base;
}

ChiSink* chiStderrColor(ChiSinkColor CHI_UNUSED(color)) {
    return chiStderr;
}

ChiSink* chiStdoutColor(ChiSinkColor CHI_UNUSED(color)) {
    return chiStdout;
}

ChiSink *const chiStdout = &sinkOut.base, *const chiStderr = &sinkErr.base;
#endif

ChiSink* chiSinkFileTryNew(const char* file, ChiSinkColor color) {
    if (streq(file, "stdout") || streq(file, "stderr"))
        return streq(file, "stderr") ? chiStderrColor(color) : chiStdoutColor(color);
    return sinkFileTryNew(file, color);
}

ChiSink* chiSinkLockNew(ChiSink* sink, size_t cap) {
    if (!sink)
        return 0;
    SinkLock* s = (SinkLock*)chiAlloc(sizeof (SinkLock) + cap);
    chiRWLockInit(&s->lock);
    s->sink = sink;
    s->used = 0;
    s->capacity = cap;
    s->base.write = sinkLockWrite;
    s->base.close = sinkLockClose;
    s->base.flush = sinkLockFlush;
    return &s->base;
}

ChiSink* chiSinkBufferNew(ChiSink* sink, size_t cap) {
    if (!cap || !sink)
        return sink;
    SinkBuffer* s = (SinkBuffer*)chiAlloc(sizeof (SinkBuffer) + cap);
    s->sink = sink;
    s->used = 0;
    s->capacity = cap;
    s->base.write = sinkBufferWrite;
    s->base.close = sinkBufferClose;
    s->base.flush = sinkBufferFlush;
    return &s->base;
}

#define NOVEC
#define VEC ChiByteVec
#define VEC_TYPE uint8_t
#define VEC_PREFIX byteVec
#include "vector.h"

static void sinkStringWrite(ChiSink* sink, const void* buf, size_t size) {
    byteVecAppend(&((ChiSinkString*)sink)->vec, (const uint8_t*)buf, size);
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
    CHI_CLEAR(&s->vec);
    s->base.write = sinkStringWrite;
    s->base.close = sinkStringClose;
    s->base.flush = sinkNullNop;
    return &s->base;
}

ChiSink* chiSinkStringNew(void) {
    ChiSinkString* s = chiAllocObj(ChiSinkString);
    chiSinkStringInit(s);
    s->base.close = sinkStringFree;
    return &s->base;
}

static ChiSink sinkNull =
    { .write = sinkNullWrite,
      .flush = sinkNullNop,
      .close = sinkNullNop
    };
ChiSink *const chiSinkNull = &sinkNull;

static void sinkMemWrite(ChiSink* sink, const void* buf, size_t size) {
    ChiSinkMem* s = (ChiSinkMem*)sink;
    size_t n = s->used + size < s->capacity ? size : (size_t)(s->capacity - s->used);
    memcpy(s->buf + s->used, buf, n);
    s->used += n;
}

ChiSink* chiSinkMemInit(ChiSinkMem* s, void* buf, size_t cap) {
    s->base.write = sinkMemWrite;
    s->base.close = sinkNullNop;
    s->base.flush = sinkNullNop;
    s->buf = (char*)buf;
    s->used = 0;
    s->capacity = cap;
    return &s->base;
}

ChiSink* chiSinkMemNew(void* buf, size_t cap) {
    ChiSinkMem* s = chiAllocObj(ChiSinkMem);
    chiSinkMemInit(s, buf, cap);
    s->base.close = sinkFree;
    return &s->base;
}

/**
 * Write quoted string using JSON-like escaping.
 * Control characters are escaped. UTF-8 characters are not escaped.
 */
size_t chiSinkWriteq(ChiSink* sink, const uint8_t* bytes, size_t size) {
    char out[512] = { '"' };
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
                out[n++] = chiHexDigit[(c / 16) & 15];
                out[n++] = chiHexDigit[c & 15];
            }
        }
    }
    out[n++] = '"';
    chiSinkWrite(sink, out, n);
    return written + n;
}

size_t chiSinkWriteh(ChiSink* sink, const void* buf, size_t size) {
    char out[512] = { '"' };
    size_t n = 1;
    for (const uint8_t *p = (const uint8_t*)buf; p < (const uint8_t*)buf + size; ++p) {
        if (n + 3 > sizeof (out)) {
            chiSinkWrite(sink, out, n);
            n = 0;
        }
        out[n++] = chiHexDigit[(*p / 16) & 15];
        out[n++] = chiHexDigit[*p & 15];
    }
    out[n++] = '"';
    chiSinkWrite(sink, out, n);
    return 2 * size + 2;
}

size_t chiSinkPuti(ChiSink* sink, int64_t x) {
    char buf[CHI_INT_BUFSIZE];
    size_t n = (size_t)(chiShowInt(buf, x) - buf);
    chiSinkWrite(sink, buf, n);
    return n;
}

size_t chiSinkPutu(ChiSink* sink, uint64_t x) {
    char buf[CHI_INT_BUFSIZE];
    size_t n = (size_t)(chiShowUInt(buf, x) - buf);
    chiSinkWrite(sink, buf, n);
    return n;
}

size_t chiSinkPutx(ChiSink* sink, uint64_t x) {
    char buf[CHI_INT_BUFSIZE];
    size_t n = (size_t)(chiShowHexUInt(buf, x) - buf);
    chiSinkWrite(sink, buf, n);
    return n;
}

CHI_COLD void chiSinkWarnv(ChiSink* sink, const char* fmt, va_list ap) {
    chiSinkPuts(sink, "Error: ");
    chiSinkFmtv(sink, fmt, ap);
    chiSinkPutc(sink, '\n');
}

CHI_COLD void chiSinkWarn(ChiSink* sink, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkWarnv(sink, fmt, ap);
    va_end(ap);
}
