#include <math.h>
#include "color.h"
#include "error.h"
#include "event.h"
#include "location.h"
#include "mem.h"
#include "runtime.h"
#include "sink.h"
#include "strutil.h"
#include "thread.h"

#if CHI_EVENT_ENABLED

#define WRITE_RAWBYTES(s)     WRITE_OP(RawBytes, (s))
#define WRITE_RAWSTR(s)       WRITE_OP(RawString, chiStringRef(s))
#define WRITE_ESCBYTES(s)     WRITE_OP(EscBytes, (s))
#define WRITE_ESCSTR(s)       WRITE_OP(EscString, chiStringRef(s))
#define WRITE_DEC(x)          WRITE_OP(Dec, (x))
#define WRITE_HEX(x)          WRITE_OP(Hex, (x))
#define WRITE_LEB128(x)       WRITE_OP(Leb128, (x))
#define WRITE_CHAR(x)         WRITE_OP(Byte, (x))
#define WRITE_BYTE(x)         WRITE_OP(Byte, (x))
#define WRITE_OP(fn, x)       ({ dst = CHI_CAT(WRITE_MODE, fn)(dst, (x)); })

static CHI_WU uint8_t* writeRawString(uint8_t* dst, ChiStringRef s) {
    memcpy(dst, s.bytes, s.size);
    return dst + s.size;
}

static CHI_WU uint8_t* estimateRawString(uint8_t* dst, ChiStringRef s) {
    return dst + s.size;
}

static CHI_WU uint8_t* writeRawBytes(uint8_t* dst, ChiBytesRef s) {
    memcpy(dst, s.bytes, s.size);
    return dst + s.size;
}

static CHI_WU uint8_t* estimateRawBytes(uint8_t* dst, ChiBytesRef s) {
    return dst + s.size;
}

static CHI_WU uint8_t* writeDec(uint8_t* dst, uint64_t n) {
    return (uint8_t*)chiShowUInt((char*)dst, n);
}

static CHI_WU uint8_t* estimateDec(uint8_t* dst, uint64_t CHI_UNUSED(n)) {
    return dst + CHI_INT_BUFSIZE;
}

static CHI_WU uint8_t* writeHex(uint8_t* dst, uint64_t n) {
    return (uint8_t*)chiShowHexUInt((char*)dst, n);
}

static CHI_WU uint8_t* estimateHex(uint8_t* dst, uint64_t CHI_UNUSED(n)) {
    return dst + CHI_INT_BUFSIZE;
}

static CHI_WU uint8_t* writeLeb128(uint8_t* dst, uint64_t n) {
    do {
        uint8_t b = (uint8_t)(n & 0x7F);
        n >>= 7;
        *dst++ = n ? (uint8_t)(b | 0x80) : b;
    } while (n);
    return dst;
}

static CHI_WU uint8_t* estimateLeb128(uint8_t* dst, uint64_t CHI_UNUSED(n)) {
    return dst + 10;
}

static CHI_WU uint8_t* writeByte(uint8_t* dst, uint32_t x) {
    CHI_ASSERT(x <= 0xFF);
    *dst++ = (uint8_t)x;
    return dst;
}

static CHI_WU uint8_t* estimateByte(uint8_t* dst, uint32_t CHI_UNUSED(x)) {
    return dst + 1;
}

static CHI_WU uint8_t* writeEscString(uint8_t* dst, ChiStringRef s) {
    for (size_t i = 0; i < s.size; ++i) {
        switch ((char)s.bytes[i]) {
        case '"': *dst++ = '\\'; *dst++ = '"'; break;
        case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
        case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
        default: *dst++ = s.bytes[i]; break;
        }
    }
    return dst;
}

static CHI_WU uint8_t* estimateEscString(uint8_t* dst, ChiStringRef s) {
    return dst + 2 * s.size;
}

static CHI_WU uint8_t* writeEscBytes(uint8_t* dst, ChiBytesRef s) {
    for (size_t i = 0; i < s.size; ++i) {
        *dst++ = (uint8_t)chiHexDigits[(s.bytes[i] / 16) & 15];
        *dst++ = (uint8_t)chiHexDigits[s.bytes[i] & 15];
    }
    return dst;
}

static CHI_WU uint8_t* estimateEscBytes(uint8_t* dst, ChiBytesRef s) {
    return dst + 2 * s.size;
}

typedef enum {
    CTX_THREAD,
    CTX_LIBRARY = CTX_THREAD,
    CTX_PROCESSOR,
    CTX_RUNTIME,
    CTX_WORKER,
} EventContext;

typedef enum {
    CLASS_BEGIN,
    CLASS_END,
    CLASS_INSTANT,
} EventClass;

#include "event/desc.h"

typedef struct {
    const ChiEventPayload* payload;
    ChiNanos ts, dur;
    uint32_t wid, tid;
    ChiEvent type;
} Event;

CHI_WARN_OFF(c++-compat)
typedef struct Buffer_ {
    struct Buffer_* next;
    uint8_t buf[0];
} Buffer;
CHI_WARN_ON

typedef struct RuntimeStats_ RuntimeStats;
typedef struct WorkerStats_ WorkerStats;

struct ChiEventState_ {
    ChiSink* sink;
    ChiNanos begin[_CHI_EVENT_DURATION_RUNTIME];
    _Atomic(Buffer*) bufList;
    CHI_IF(CHI_STATS_ENABLED, RuntimeStats* stats;)
};

struct ChiEventWS_ {
    ChiNanos begin[_CHI_EVENT_DURATION_WORKER];
    Buffer*  buf;
    CHI_IF(CHI_STATS_ENABLED, WorkerStats* stats;)
};

#if CHI_STATS_ENABLED
#  include "event/stats.h"
#endif

static Buffer* bufferAcquire(ChiEventState* state, ChiWorker* worker, size_t msgSize) {
    if (worker) {
        ChiEventWS* ws = worker->eventWS;
        if (CHI_UNLIKELY(!ws->buf))
            ws->buf = (Buffer*)chiZalloc(msgSize);
        return ws->buf;
    }
    for (;;) {
        Buffer* buf = atomic_load_explicit(&state->bufList, memory_order_acquire);
        if (!buf)
            return (Buffer*)chiZalloc(msgSize);
        if (atomic_compare_exchange_weak_explicit(&state->bufList, &buf, buf->next,
                                                  memory_order_acq_rel,
                                                  memory_order_relaxed))
            return buf;
    }
}

static void bufferRelease(ChiEventState* state, ChiWorker* worker, Buffer* buf) {
    if (worker)
        return;
    do {
        buf->next = atomic_load_explicit(&state->bufList, memory_order_acquire);
    } while (!atomic_compare_exchange_weak_explicit(&state->bufList, &buf->next, buf,
                                                    memory_order_acq_rel,
                                                    memory_order_relaxed));
}

static void bufferDestroy(ChiEventState* state) {
    Buffer* buf = atomic_load_explicit(&state->bufList, memory_order_relaxed);
    while (buf) {
        Buffer* next = buf->next;
        chiFree(buf);
        buf = next;
    }
}

#define WRITE_MODE estimate
// lint: no-sort
#include "event/write_pretty.h"
#include "event/serialize.h"
#include "event/write_bin.h"
#include "event/serialize.h"
#undef WRITE_MODE

#define WRITE_MODE write
// lint: no-sort
#include "event/write_pretty.h"
#include "event/serialize.h"
#include "event/write_bin.h"
#include "event/serialize.h"
#undef WRITE_MODE

static void eventWrite(ChiRuntime* rt, ChiWorker* worker, const Event* e) {
    ChiEventState* state = rt->eventState;
    const ChiRuntimeOption* opt = &rt->option;
    size_t size = (size_t)(CHI_AND(CHI_EVENT_PRETTY_ENABLED, opt->event.pretty) ? estimatePrettyEvent(0, e) : estimateBinEvent(0, e));
    if (size > opt->event.msgSize - sizeof (Buffer)) {
        chiWarn("Discarded overly long event of type %s.", chiEventName[e->type]);
        return;
    }
    Buffer* buf = bufferAcquire(state, worker, opt->event.msgSize);
    uint8_t* end = CHI_AND(CHI_EVENT_PRETTY_ENABLED, opt->event.pretty) ? writePrettyEvent(buf->buf, e) : writeBinEvent(buf->buf, e);
    chiSinkWrite(state->sink, buf->buf, (size_t)(end - buf->buf));
    bufferRelease(state, worker, buf);
}

#if CHI_EVENT_CONVERT_ENABLED
#define READ_CHECK(fn, ...)   ({ src = fn(src, end, ##__VA_ARGS__); if (!src) return 0; })
#define READ_BYTES(x, n)      READ_CHECK(readBytes, (x), (n))
#define READ_LEB128(x)        READ_CHECK(readLeb128, (x))
#define READ_BYTE(x)          READ_CHECK(readByte, (x))

static CHI_WU const uint8_t* readBytes(const uint8_t* src, const uint8_t* end, const uint8_t** s, size_t n) {
    if (CHI_UNLIKELY(src + n > end))
        return 0;
    *s = src;
    return src + n;
}

static CHI_WU const uint8_t* readByte(const uint8_t* src, const uint8_t* end, uint8_t* b) {
    if (CHI_UNLIKELY(src >= end))
        return 0;
    *b = *src++;
    return src;
}

static CHI_WU const uint8_t* readLeb128(const uint8_t* src, const uint8_t* end, uint64_t* x) {
    uint32_t n = 0;
    *x = 0;
    for (;;) {
        if (CHI_UNLIKELY(src >= end))
            return 0;
        uint8_t b = *src++;
        *x |= (uint64_t)(b & 0x7FU) << n;
        if (!(b & 0x80U))
            break;
        n += 7;
    }
    return src;
}

// lint: no-sort
#include "event/read_bin.h"
#include "event/serialize.h"

#define WRITE_MODE estimate
// lint: no-sort
#include "event/write_csv.h"
#include "event/serialize.h"
#include "event/write_json.h"
#include "event/serialize.h"
#undef WRITE_MODE

#define WRITE_MODE write
// lint: no-sort
#include "event/write_csv.h"
#include "event/serialize.h"
#include "event/write_json.h"
#include "event/serialize.h"
#undef WRITE_MODE

void chiEventConvert(const char* inName, ChiFile in, ChiSink* out, ChiEventFormat fmt, size_t bufSize) {
    CHI_AUTO_ALLOC(uint8_t, inBuf, bufSize);
    CHI_AUTO_ALLOC(uint8_t, outBuf, bufSize);
    size_t inPos = 0, outPos = 0, inRead = 0;
    ChiEventPayload payload;
    Event e = { .payload = &payload };
    for (;;) {
        size_t read;
        if (!chiFileRead(in, inBuf + inRead, bufSize - inRead, &read))
            chiErr("Reading from file '%s' failed: %m", inName);
        inRead += read;
        if (inRead == 0) {
            if (outPos)
                chiSinkWrite(out, outBuf, outPos);
            return;
        }

        for (;;) {
            const uint8_t* inPtr = readBinEvent(inBuf + inPos, inBuf + inRead, &e);
            if (!inPtr) {
                if (inPos == 0)
                    chiErr("Unexpected end or invalid data in '%s'", inName);
                inRead -= inPos;
                memmove(inBuf, inBuf + inPos, inRead);
                inPos = 0;
                break;
            }
            inPos = (size_t)(inPtr - inBuf);

            size_t size = 0;
            switch (fmt) {
            case CHI_EVFMT_JSON: size = (size_t)estimateJsonEvent(0, &e); break;
            case CHI_EVFMT_CSV: size = (size_t)estimateCsvEvent(0, &e); break;
            case CHI_EVFMT_PRETTY: size = (size_t)estimatePrettyEvent(0, &e); break;
            case CHI_EVFMT_BIN: size = (size_t)estimateBinEvent(0, &e); break;
            case CHI_EVFMT_NONE: default: CHI_BUG("Invalid event format");
            }

            if (size > bufSize - outPos) {
                if (outPos) {
                    chiSinkWrite(out, outBuf, outPos);
                    outPos = 0;
                }
                if (size > bufSize)
                    chiErr("Message too long to write in '%s'", inName);
            }

            uint8_t* outPtr = 0;
            switch (fmt) {
            case CHI_EVFMT_JSON: outPtr = writeJsonEvent(outBuf + outPos, &e); break;
            case CHI_EVFMT_CSV: outPtr = writeCsvEvent(outBuf + outPos, &e); break;
            case CHI_EVFMT_PRETTY: outPtr = writePrettyEvent(outBuf + outPos, &e); break;
            case CHI_EVFMT_BIN: outPtr = writeBinEvent(outBuf + outPos, &e); break;
            case CHI_EVFMT_NONE: default: CHI_BUG("Invalid event format");
            }
            outPos = (size_t)(outPtr - outBuf);
        }
    }
}
#endif

void chiEventWrite(void* ctx, ChiEvent e, const ChiEventPayload* p) {
    CHI_ASSERT(ctx);

    ChiProcessor* proc = 0;
    ChiWorker* worker = 0;
    ChiRuntime* rt;
    if (eventDesc[e].ctx == CTX_THREAD || eventDesc[e].ctx == CTX_PROCESSOR) {
        proc = (ChiProcessor*)ctx;
        worker = proc->worker;
        rt = worker->rt;
    } else if (eventDesc[e].ctx == CTX_WORKER) {
        worker = (ChiWorker*)ctx;
        rt = worker->rt;
    } else {
        rt = (ChiRuntime*)ctx;
    }

    CHI_ASSERT(e == CHI_EVENT_LOG_BEGIN || e == CHI_EVENT_LOG_END ||
               chiEventFilterEnabled(rt->option.event.filter, e));

    ChiEventState* state = rt->eventState;
    if (!state)
        return;

    Event event = {
        .type = e,
        .payload = p,
        .ts = chiNanosDelta(chiClockFine(), rt->timeRef.start),
        .wid = worker ? worker->wid : 0,
        .tid = eventDesc[e].ctx == CTX_THREAD ? chiThreadId(proc->thread) : 0,
    };

    if (eventDesc[e].cls == CLASS_BEGIN)
        eventDesc[e].begin[worker ? worker->eventWS->begin : state->begin] = event.ts;
    else if (eventDesc[e].cls == CLASS_END)
        event.dur = chiNanosDelta(event.ts, eventDesc[e].begin[worker ? worker->eventWS->begin : state->begin]);

    CHI_IF(CHI_STATS_ENABLED, statsCollect(state->stats, worker ? worker->eventWS->stats : 0, &event));

    if (eventDesc[e].cls != CLASS_BEGIN && state->sink)
        eventWrite(rt, worker, &event);
}

void chiEventWorkerStart(ChiWorker* worker) {
    ChiEventState* state = worker->rt->eventState;
    if (!state)
        return;

    worker->eventWS = chiZallocObj(ChiEventWS);
    CHI_IF(CHI_STATS_ENABLED, worker->eventWS->stats = statsWorkerStart(state->stats));

    chiEvent(worker, WORKER_NAME, .name = chiStringRef(worker->name));
    chiEvent0(worker, WORKER_INIT);
}

void chiEventWorkerStop(ChiWorker* worker) {
    if (worker->eventWS) {
        chiEvent0(worker, WORKER_DESTROY);
        CHI_IF(CHI_STATS_ENABLED, statsWorkerStop(worker->rt->eventState->stats, worker->eventWS->stats));
        chiFree(worker->eventWS->buf);
        chiFree(worker->eventWS);
    }
}

void chiEventDestroy(ChiRuntime* rt) {
    ChiEventState* state = rt->eventState;
    if (!state)
        return;

    chiEvent0(rt, SHUTDOWN_END);

    // Always write the end event to ensure proper log finalization
    chiEventWrite(rt, CHI_EVENT_LOG_END, 0);

    if (state->sink) {
        chiSinkClose(state->sink);
        bufferDestroy(state);
    }

    CHI_IF(CHI_STATS_ENABLED, statsDestroy(rt));

    chiFree(state);
    rt->eventState = 0;
}

void chiEventSetup(ChiRuntime* rt) {
    const ChiRuntimeOption* opt = &rt->option;

    if (!CHI_AND(CHI_STATS_ENABLED, opt->stats.enabled) && !opt->event.enabled) {
        CHI_ZERO_ARRAY(rt->option.event.filter);
        return;
    }

    rt->eventState = chiZallocObj(ChiEventState);

    if (opt->event.enabled) {
        ChiSinkColor color = CHI_AND(CHI_COLOR_ENABLED,
                                     CHI_AND(CHI_EVENT_PRETTY_ENABLED, opt->event.pretty)
                                     ? opt->color : CHI_SINK_BINARY);
        rt->eventState->sink = chiSinkFileTryNew(opt->event.file, opt->event.bufSize * opt->event.msgSize, true, color);
    }

    CHI_IF(CHI_STATS_ENABLED, rt->eventState->stats = statsSetup(opt));

    // Always write the begin event to ensure proper log initialization
    chiEventWrite(rt, CHI_EVENT_LOG_BEGIN, 0);
    chiEvent0(rt, STARTUP_BEGIN);
}

bool chiEventModifyFilter(void* filter, ChiStringRef s) {
    const size_t filterSize = _CHI_EVENT_FILTER_SIZE * sizeof (uintptr_t);
    const uint8_t* p = s.bytes, *end = p + s.size;
    while (p < end) {
        bool overwrite = *p != '-' && *p != '+';
        bool set = overwrite || *p++ == '+';
        if (overwrite)
            memset(filter, 0, filterSize);

        size_t len = (size_t)(end - p);
        const uint8_t
            *nextPlus = (uint8_t*)memchr(p, '+', len),
            *nextMinus = (uint8_t*)memchr(p, '-', len),
            *next =
            nextPlus && nextMinus ? CHI_MIN(nextPlus, nextMinus) :
            nextPlus ? nextPlus :
            nextMinus ? nextMinus : end;

        len = (size_t)(next - p);
        if (!len) {
            chiWarn("Invalid empty event filter pattern");
            return false;
        }

        if (memeqstr(p, len, "ALL")) {
            memset(filter, set ? 0xFF : 0, filterSize);
        } else {
            bool found = false;
            for (size_t i = 0; i < _CHI_EVENT_COUNT; ++i) {
                if (!strncmp(chiEventName[i], (const char*)p, len)) {
                    chiBitSet((uintptr_t*)filter, i, set);
                    found = true;
                }
            }
            if (!found) {
                chiWarn("Invalid event filter pattern '%b'", (uint32_t)len, p);
                return false;
            }
        }
        p = next;
    }

    return true;
}

const void* chiEventFilter(void) {
    return CHI_CURRENT_RUNTIME->option.event.filter;
}

#include "event/stubs.h"

#if CHI_LTTNG_ENABLED
#  define TRACEPOINT_CREATE_PROBES
#  define TRACEPOINT_DEFINE
#  define TRACEPOINT_HEADER_MULTI_READ
#  include "event/lttng.h" // lint: keep
#endif

#endif

#if CHI_FNLOG_ENABLED
void chiFnLog(ChiCont cont) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    ChiLocResolve resolve;
    chiLocResolve(&resolve, (ChiLoc){ .type = CHI_LOC_NATIVE, .cont = cont },
                  proc->rt->option.stack.traceMangled);
    chiEventStruct(proc, FNLOG_CONT, &resolve.loc);
}
#endif
