#define SPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, Pretty##n)(uint8_t* dst, bool* prettyFirst, const p) {
#define SPAYLOAD_END     return dst; }
#define SEVENT_BEGIN                                                    \
    static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, PrettyEvent)(uint8_t* dst, const Event* e) { \
    bool prettyFirst_ = false, *prettyFirst = &prettyFirst_;            \
    WRITE_RAWSTR(FgBlue); WRITE_RAWSTR(chiEventName[e->type]); WRITE_RAWSTR(FgDefault); \
    for (size_t _n = _CHI_EVENT_MAXLEN - strlen(chiEventName[e->type]); _n --> 0;) WRITE_CHAR(' ');
#define SEVENT_END       WRITE_CHAR('\n'); return dst; }
#define SPAYLOAD(n, p)   dst = CHI_CAT(WRITE_MODE, Pretty##n)(dst, prettyFirst, (p))
#define SSTR(x)          ({ WRITE_RAWSTR(FgMagenta "\""); WRITE_ESCSTR(x); WRITE_RAWSTR("\"" FgDefault); })
#define SBYTES(x)        ({ WRITE_RAWSTR(FgMagenta "\""); WRITE_ESCBYTES(x); WRITE_RAWSTR("\"" FgDefault); })
#define SBLOCK_BEGIN(n)  ({ if (!*prettyFirst) { WRITE_CHAR(' '); *prettyFirst = true; } WRITE_RAWSTR(FgCyan CHI_STRINGIZE(n) FgDefault "=["); })
#define SFIELD(k, v)     ({ if (*prettyFirst) *prettyFirst = false; else WRITE_CHAR(' '); WRITE_RAWSTR(FgGreen CHI_STRINGIZE(k) FgDefault "="); v; })
#define SBLOCK_END(n)    WRITE_CHAR(']')
#define SDEC(n)          ({ WRITE_RAWSTR(FgRed); WRITE_DEC(n); WRITE_RAWSTR(FgDefault); })
#define SHEX(n)          ({ WRITE_RAWSTR(FgRed "0x"); WRITE_HEX(n); WRITE_RAWSTR(FgDefault); })
#define SBOOL(x)         ({ WRITE_RAWSTR((x) ? FgYellow"True"FgDefault : FgYellow"False"FgDefault); })
#define SENUM(e,n)       ({ WRITE_RAWSTR(FgYellow); WRITE_RAWSTR(enum##e[n]); WRITE_RAWSTR(FgDefault); })
