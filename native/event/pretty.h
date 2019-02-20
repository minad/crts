#define XFORMAT          pretty
#define XSTR(x)          ({ RAWSTR(FgRed "\""); CHECK(jsonEscape, (x)); RAWSTR("\"" FgDefault); })
#define XBLOCK_BEGIN(n)  ({ if (!*xstate) { CHAR(' '); *xstate = true; } RAWSTR(FgCyan n FgDefault "=["); })
#define XUBLOCK_BEGIN(n) ({ if (!*xstate) { CHAR(' '); *xstate = true; } CHAR('['); })
#define XFIELD(k, v)     ({ if (*xstate) *xstate = false; else CHAR(' '); RAWSTR(FgGreen k FgDefault "="); v; })
#define XBLOCK_END(n)    CHAR(']')
#define XUBLOCK_END(n)   CHAR(']')
#define XINIT            bool first = false; bool* xstate = &first;
#define XSTATE           bool*
#define XEVENT_BEGIN(e)  ({ RAWSTR(FgBlue); RAWSTR(chiEventName[e]); RAWSTR(FgDefault); CHECK(logAppend, "                              ", _CHI_EVENT_MAXLEN - strlen(chiEventName[e])); })
#define XEVENT_END(e)    CHAR('\n')
#define XNUM(n)          ({ RAWSTR(FgRed); NUM(n); RAWSTR(FgDefault); })
#define XSNUM(n)         ({ RAWSTR(FgRed); SNUM(n); RAWSTR(FgDefault); })
#define XENUM(e,n)       ({ RAWSTR(FgYellow); RAWSTR(enum##e[n]); RAWSTR(FgDefault); })
