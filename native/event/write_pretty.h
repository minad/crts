#define WPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* writePretty##n(uint8_t* dst, uint8_t* end, bool* prettyFirst, const p) {
#define WPAYLOAD_END     return dst; }
#define WEVENT_BEGIN                                                    \
    static CHI_WU uint8_t* writePrettyEvent(uint8_t* dst, uint8_t* end, const Event* e) { \
    bool prettyFirst_ = false, *prettyFirst = &prettyFirst_;            \
    WRITE_RAWSTR(FgBlue); WRITE_RAWSTR(chiEventName[e->type]); WRITE_RAWSTR(FgDefault); \
    for (size_t _n = _CHI_EVENT_MAXLEN - strlen(chiEventName[e->type]); _n --> 0;) WRITE_CHAR(' ');
#define WEVENT_END       if (dst) WRITE_CHAR('\n'); return dst; }
#define WPAYLOAD(n, p)   dst = writePretty##n(dst, end, prettyFirst, (p))
#define WSTR(x)          ({ WRITE_RAWSTR(FgMagenta "\""); WRITE_ESCAPESTR(x); WRITE_RAWSTR("\"" FgDefault); })
#define WBYTES(x)        ({ WRITE_RAWSTR(FgMagenta "\""); WRITE_ESCAPEBYTES(x); WRITE_RAWSTR("\"" FgDefault); })
#define WBLOCK_BEGIN(n)  ({ if (!*prettyFirst) { WRITE_CHAR(' '); *prettyFirst = true; } WRITE_RAWSTR(FgCyan CHI_STRINGIZE(n) FgDefault "=["); })
#define WFIELD(k, v)     ({ if (*prettyFirst) *prettyFirst = false; else WRITE_CHAR(' '); WRITE_RAWSTR(FgGreen CHI_STRINGIZE(k) FgDefault "="); v; })
#define WBLOCK_END(n)    WRITE_CHAR(']')
#define WDEC(n)          ({ WRITE_RAWSTR(FgRed); WRITE_DEC(n); WRITE_RAWSTR(FgDefault); })
#define WHEX(n)          ({ WRITE_RAWSTR(FgRed "0x"); WRITE_HEX(n); WRITE_RAWSTR(FgDefault); })
#define WBOOL(x)         ({ WRITE_RAWSTR((x) ? FgYellow"True"FgDefault : FgYellow"False"FgDefault); })
#define WENUM(e,n)       ({ WRITE_RAWSTR(FgYellow); WRITE_RAWSTR(enum##e[n]); WRITE_RAWSTR(FgDefault); })
