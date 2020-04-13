#pragma once

#include <stdarg.h>
#include "rtoption.h"
#include "system.h"

typedef struct ChiSink_ ChiSink;
typedef struct ChiProcessor_ ChiProcessor;

/**
 * Sink for outputting data.
 * This datastructure contains the vtable of
 * sink operations.
 */
struct ChiSink_ {
    bool (*write)(ChiSink*, const void*, size_t);
    bool (*flush)(ChiSink*);
    void (*close)(ChiSink*);
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

typedef struct {
    ChiSink  base;
    ChiSink* sink;
} _ChiSinkProxy;

typedef struct {
    ChiSink    base;
    ChiFile    handle;
} _ChiSinkFile;

#define chiStdout (&_chiStdout.base)
#define chiStderr (&_chiStderr.base)
#define chiSinkNull CHI_CONST_CAST(&_chiSinkNull, ChiSink*)

CHI_EXTERN _ChiSinkProxy _chiStdout;
CHI_EXTERN _ChiSinkFile _chiStderr;
CHI_EXTERN const ChiSink _chiSinkNull;

CHI_INTERN ChiSink* chiSinkColor(ChiSinkColor);
CHI_INTERN ChiSink* chiSinkMemInit(ChiSinkMem*, void*, size_t);
CHI_INTERN ChiSink* chiSinkStringInit(ChiSinkString*);
CHI_INTERN CHI_WU ChiSink* chiSinkStringNew(void);
CHI_INTERN CHI_WU ChiSink* chiSinkFileTryNew(const char*, size_t, bool, ChiSinkColor);
CHI_INTERN ChiStringRef chiSinkString(ChiSink*);
CHI_INTERN const char* chiSinkCString(ChiSink*);
CHI_INTERN void chiSinkWarn(ChiSink*, const char*, ...) CHI_FMT(2, 3);
CHI_INTERN void chiSinkWarnv(ChiSink*, const char*, va_list);
CHI_INTERN size_t chiSinkWriteq(ChiSink*, const uint8_t*, size_t);
CHI_INTERN size_t chiSinkWriteh(ChiSink*, const void*, size_t);
CHI_INTERN size_t chiSinkFmt(ChiSink*, const char*, ...) CHI_FMT(2, 3);
CHI_INTERN size_t chiSinkFmtv(ChiSink*, const char*, va_list);
CHI_INTERN size_t chiSinkPutu(ChiSink*, uint64_t);
CHI_INTERN void chiSinkFlush(ChiSink*);
CHI_INTERN void chiSinkWrite(ChiSink*, const void*, size_t);
CHI_INTERN void chiSinkClose(ChiSink*);

CHI_INL size_t chiSinkPuts(ChiSink* sink, ChiStringRef s) {
    chiSinkWrite(sink, s.bytes, s.size);
    return s.size;
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

CHI_INTERN size_t chiFmtv(char*, size_t, const char*, va_list);
CHI_INTERN size_t chiFmt(char*, size_t, const char*, ...) CHI_FMT(3, 4);
CHI_INTERN Chili chiFmtStringv(ChiProcessor*, const char*, va_list);
CHI_INTERN Chili chiFmtString(ChiProcessor*, const char*, ...) CHI_FMT(1, 2);
