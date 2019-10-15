#define WSTR(x)          ({ WRITE_CHAR('"'); WRITE_ESCAPESTR(x); WRITE_CHAR('"'); })
#define WBYTES(x)        ({ WRITE_CHAR('"'); WRITE_ESCAPEBYTES(x); WRITE_CHAR('"'); })
#define WFIELD(k, v)     ({ if (*jsonFirst) *jsonFirst = false; else WRITE_CHAR(','); WRITE_RAWSTR("\"" CHI_STRINGIZE(k) "\":"); v; })
#define WBLOCK_BEGIN(n)  ({ if (!*jsonFirst) { WRITE_CHAR(','); *jsonFirst = true; } WRITE_RAWSTR("\"" CHI_STRINGIZE(n) "\":{"); })
#define WBLOCK_END(n)    WRITE_CHAR('}')
#define WEVENT_BEGIN                                                    \
    static CHI_WU uint8_t* writeJsonEvent(uint8_t* dst, uint8_t* end, const Event* e) { \
    bool jsonFirst_ = true, *jsonFirst = &jsonFirst_;                  \
    WRITE_CHAR(e->type == CHI_EVENT_LOG_BEGIN ? '[' : ','); \
    WRITE_CHAR('{'); WFIELD(e, WRITE_CHAR('"'); WRITE_RAWSTR(chiEventName[e->type]); WRITE_CHAR('"'));
#define WEVENT_END       if (dst) WRITE_RAWSTR(e->type == CHI_EVENT_LOG_END ? "}]\n" : "}\n"); return dst; }
#define WPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* writeJson##n(uint8_t* dst, uint8_t* end, bool* jsonFirst, const p) {
#define WPAYLOAD_END     return dst; }
#define WPAYLOAD(n, p)   dst = writeJson##n(dst, end, jsonFirst, (p))
#define WDEC(n)          WRITE_DEC(n)
#define WHEX(n)          WDEC(n)
#define WBOOL(x)         WRITE_RAWSTR((x) ? "true" : "false")
#define WENUM(e,n)       ({ WRITE_CHAR('"'); WRITE_RAWSTR(enum##e[n]); WRITE_CHAR('"'); })
