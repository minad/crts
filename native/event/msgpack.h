#define XFORMAT          mp
#define XSTR(x)          CHECK(mpString, (x))
#define XUBLOCK_BEGIN(n) ({})
#define XUBLOCK_END(n)   ({})
#define XFIELD(k, v)     ({ ++(*xstate); v; })
#define XBLOCK_BEGIN(n)  ({})
#define XBLOCK_END(n)    ({})
#define XINIT            uint32_t count = 0; uint8_t* len; uint32_t* xstate = &count;
#define XSTATE           uint32_t*
#define XEVENT_BEGIN(e)  ({ *log->ptr++ = 0xdc; len = log->ptr; *log->ptr++ = 0; *log->ptr++ = 0; XFIELD("e", XNUM(e)); })
#define XEVENT_END(e)    ({ len[0] = (uint8_t)(count >> 8); len[1] = (uint8_t)count; })
#define XNUM(n)          CHECK(mpUInt, (n))
#define XSNUM(n)         CHECK(mpInt, (n))
#define XENUM(_,n)       XNUM(n)

static CHI_WU bool mpString(Log* log, ChiStringRef s) {
    if (!logCheck(log, 5 + s.size))
        return false;
    if (s.size <= 0x1F) {
        *log->ptr++ = (uint8_t)(0xa0 | s.size);
        memcpy(log->ptr, s.bytes, s.size);
        log->ptr += s.size;
    } else if (s.size <= UINT8_MAX) {
        *log->ptr++ = 0xd9;
        *log->ptr++ = (uint8_t)s.size;
        memcpy(log->ptr, s.bytes, s.size);
        log->ptr += s.size;
    } else if (s.size <= UINT16_MAX) {
        *log->ptr++ = 0xda;
        *log->ptr++ = (uint8_t)(s.size >> 8);
        *log->ptr++ = (uint8_t)s.size;
        memcpy(log->ptr, s.bytes, s.size);
        log->ptr += s.size;
    } else {
        *log->ptr++ = 0xd9;
        *log->ptr++ = (uint8_t)(s.size >> 24);
        *log->ptr++ = (uint8_t)(s.size >> 16);
        *log->ptr++ = (uint8_t)(s.size >> 8);
        *log->ptr++ = (uint8_t)s.size;
        memcpy(log->ptr, s.bytes, s.size);
        log->ptr += s.size;
    }
    return true;
}

static CHI_WU bool mpUInt(Log* log, uint64_t n) {
    if (!logCheck(log, 9))
        return false;
    if (n <= UINT8_MAX) {
        *log->ptr++ = 0xcc;
        *log->ptr++ = (uint8_t)n;
    } else if (n <= UINT16_MAX) {
        *log->ptr++ = 0xcd;
        *log->ptr++ = (uint8_t)(n >> 8);
        *log->ptr++ = (uint8_t)n;
    } else if (n <= UINT32_MAX) {
        *log->ptr++ = 0xce;
        *log->ptr++ = (uint8_t)(n >> 24);
        *log->ptr++ = (uint8_t)(n >> 16);
        *log->ptr++ = (uint8_t)(n >> 8);
        *log->ptr++ = (uint8_t)n;
    } else {
        *log->ptr++ = 0xcf;
        *log->ptr++ = (uint8_t)(n >> 56);
        *log->ptr++ = (uint8_t)(n >> 48);
        *log->ptr++ = (uint8_t)(n >> 40);
        *log->ptr++ = (uint8_t)(n >> 32);
        *log->ptr++ = (uint8_t)(n >> 24);
        *log->ptr++ = (uint8_t)(n >> 16);
        *log->ptr++ = (uint8_t)(n >> 8);
        *log->ptr++ = (uint8_t)n;
    }
    return true;
}
