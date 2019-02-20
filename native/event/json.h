#define XFORMAT          json
#define XSTR(x)          ({ CHAR('"'); CHECK(jsonEscape, (x)); CHAR('"'); })
#define XBLOCK_BEGIN(n)  ({ if (!*xstate) { CHAR(','); *xstate = true; } SAFE_QSTR(n); CHAR(':'); CHAR('{'); })
#define XUBLOCK_BEGIN(n) ({ if (!*xstate) { CHAR(','); *xstate = true; } CHAR('{'); })
#define XFIELD(k, v)     ({ if (*xstate) *xstate = false; else CHAR(','); SAFE_QSTR(k); CHAR(':'); v; })
#define XBLOCK_END(n)    CHAR('}')
#define XUBLOCK_END(n)   CHAR('}')
#define XINIT            bool first = false; bool* xstate = &first;
#define XSTATE           bool*
#define XEVENT_BEGIN(e)  ({ XEVENT_JSON_BEGIN(e); XUBLOCK_BEGIN(ignored); XFIELD("e", SAFE_QSTR(chiEventName[e])); })
#define XEVENT_END(e)    ({ XUBLOCK_END(ignored); CHAR('\n'); XEVENT_JSON_END(e); })
#define XEVENT_JSON_BEGIN(e) ({ if (e == CHI_EVENT_BEGIN) { *xstate = true; CHAR('['); } })
#define XEVENT_JSON_END(e)   ({ if (e == CHI_EVENT_END) { RAWSTR("]\n"); } })
#define XNUM(n)          NUM(n)
#define XSNUM(n)         SNUM(n)
#define XENUM(e,n)       ({ CHAR('"'); RAWSTR(enum##e[n]); CHAR('"'); })
