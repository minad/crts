#define SSTR(x)          ({ WRITE_CHAR('"'); WRITE_ESCSTR(x); WRITE_CHAR('"'); })
#define SBYTES(x)        ({ WRITE_CHAR('"'); WRITE_ESCBYTES(x); WRITE_CHAR('"'); })
#define SFIELD(k, v)     ({ if (*jsonFirst) *jsonFirst = false; else WRITE_CHAR(','); WRITE_RAWSTR("\"" CHI_STRINGIZE(k) "\":"); v; })
#define SBLOCK_BEGIN(n)  ({ if (!*jsonFirst) { WRITE_CHAR(','); *jsonFirst = true; } WRITE_RAWSTR("\"" CHI_STRINGIZE(n) "\":{"); })
#define SBLOCK_END(n)    WRITE_CHAR('}')
#define SEVENT_BEGIN                                                    \
    static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, JsonEvent)(uint8_t* dst, const Event* e) { \
    bool jsonFirst_ = true, *jsonFirst = &jsonFirst_;                  \
    WRITE_CHAR(e->type == CHI_EVENT_LOG_BEGIN ? '[' : ','); \
    WRITE_CHAR('{'); SFIELD(e, WRITE_CHAR('"'); WRITE_RAWSTR(chiEventName[e->type]); WRITE_CHAR('"'));
#define SEVENT_END       WRITE_RAWSTR(e->type == CHI_EVENT_LOG_END ? "}]\n" : "}\n"); return dst; }
#define SPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, Json##n)(uint8_t* dst, bool* jsonFirst, const p) {
#define SPAYLOAD_END     return dst; }
#define SPAYLOAD(n, p)   dst = CHI_CAT(WRITE_MODE, Json##n)(dst, jsonFirst, (p))
#define SDEC(n)          WRITE_DEC(n)
#define SHEX(n)          SDEC(n)
#define SBOOL(x)         WRITE_RAWSTR((x) ? "true" : "false")
#define SENUM(e,n)       ({ WRITE_CHAR('"'); WRITE_RAWSTR(enum##e[n]); WRITE_CHAR('"'); })
