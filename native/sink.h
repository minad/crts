#pragma once

#include "rtoption.h"
#include <chili/object/string.h>
#include <stdarg.h>

typedef struct ChiSink_ ChiSink;

/**
 * Sink for outputting data.
 * This datastructure contains the vtable of
 * sink operations.
 */
struct ChiSink_ {
    void (*write)(ChiSink*, const void*, size_t);
    void (*close)(ChiSink*);
    void (*flush)(ChiSink*);
};

typedef struct {
    ChiSink base;
    char* buf;
    size_t used, capacity;
} ChiSinkMem;

typedef struct {
    size_t used, capacity;
    uint8_t* elem;
} ChiByteVec;

typedef struct {
    ChiSink    base;
    ChiByteVec vec;
} ChiSinkString;

extern ChiSink *const chiStdout;
extern ChiSink *const chiStderr;
extern ChiSink *const chiSinkNull;
CHI_WU ChiSink* chiStderrColor(ChiSinkColor);
CHI_WU ChiSink* chiStdoutColor(ChiSinkColor);

ChiSink* chiSinkMemInit(ChiSinkMem*, void*, size_t);
ChiSink* chiSinkStringInit(ChiSinkString*);
CHI_WU ChiSink* chiSinkMemNew(void*, size_t);
CHI_WU ChiSink* chiSinkStringNew(void);
CHI_WU ChiSink* chiSinkLockNew(ChiSink*, size_t);
CHI_WU ChiSink* chiSinkBufferNew(ChiSink*, size_t);
CHI_WU ChiSink* chiSinkFileTryNew(const char*, ChiSinkColor);
ChiStringRef chiSinkString(ChiSink*);
const char* chiSinkCString(ChiSink*);
void chiSinkWarn(ChiSink*, const char*, ...) CHI_FMT(2, 3);
void chiSinkWarnv(ChiSink*, const char*, va_list);
size_t chiSinkWriteq(ChiSink*, const uint8_t*, size_t);
size_t chiSinkWriteh(ChiSink*, const void*, size_t);
size_t chiSinkFmt(ChiSink*, const char*, ...) CHI_FMT(2, 3);
size_t chiSinkFmtv(ChiSink*, const char*, va_list);
size_t chiSinkPuti(ChiSink*, int64_t);
size_t chiSinkPutu(ChiSink*, uint64_t);
size_t chiSinkPutx(ChiSink*, uint64_t);
const char* chiZstdPath(void);

CHI_INL CHI_WU const char* chiSinkCompress(void) {
    return CHI_SINK_ZSTD_ENABLED && chiZstdPath() ? ".zst" : "";
}

CHI_INL void chiSinkFlush(ChiSink* sink) {
    sink->flush(sink);
}

CHI_INL void chiSinkClose(ChiSink* sink) {
    sink->close(sink);
}

CHI_INL size_t chiSinkWrite(ChiSink* sink, const void* buf, size_t size) {
    sink->write(sink, buf, size);
    return size;
}

CHI_INL size_t chiSinkPuts(ChiSink* sink, ChiStringRef s) {
    return chiSinkWrite(sink, s.bytes, s.size);
}
#define chiSinkPuts(sink, str) chiSinkPuts((sink), chiStringRef(str))

CHI_INL size_t chiSinkPutq(ChiSink* sink, ChiStringRef s) {
    return chiSinkWriteq(sink, s.bytes, s.size);
}
#define chiSinkPutq(sink, str) chiSinkPutq((sink), chiStringRef(str))

CHI_INL size_t chiSinkPuth(ChiSink* sink, ChiStringRef s) {
    return chiSinkWriteh(sink, s.bytes, s.size);
}
#define chiSinkPuth(sink, str) chiSinkPuth((sink), chiStringRef(str))

CHI_INL void chiSinkPutc(ChiSink* sink, int c) {
    chiSinkWrite(sink, &(char){(char)c}, 1);
}

CHI_DEFINE_AUTO(ChiSink*, chiSinkClose)
#define CHI_AUTO_SINK(sink, ...) CHI_AUTO(sink, (__VA_ARGS__), chiSinkClose)
#define _CHI_STRING_SINK(t, s)                                       \
    ChiSinkString t;                                                 \
    CHI_AUTO_SINK(s, chiSinkStringInit(&t))
#define CHI_STRING_SINK(s) _CHI_STRING_SINK(CHI_GENSYM, s)

size_t chiFmt(char*, size_t, const char*, ...) CHI_FMT(3, 4);
size_t chiFmtv(char*, size_t, const char*, va_list);
Chili chiFmtString(const char*, ...) CHI_FMT(1, 2);
Chili chiFmtStringv(const char*, va_list);
