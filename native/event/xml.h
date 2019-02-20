#define XFORMAT          xml
#define XSTR(x)          ({ CHAR('"'); CHECK(xmlEscape, (x)); CHAR('"'); })
#define XBLOCK_BEGIN(n)  ({ if (*xstate) { CHAR('>'); *xstate = false; }RAWSTR("<" n); *xstate = true; })
#define XUBLOCK_BEGIN(n) XBLOCK_BEGIN(n)
#define XBLOCK_END(n)    ({ if (*xstate) { RAWSTR("/>"); *xstate = false; } else RAWSTR("</" n ">"); })
#define XUBLOCK_END(n)   XBLOCK_END(n)
#define XFIELD(k, v)     ({ CHI_ASSERT(*xstate); RAWSTR(" " k "=\""); v; CHAR('"'); })
#define XINIT            bool attrs = true; bool* xstate = &attrs;
#define XSTATE           bool*
#define XEVENT_BEGIN(e)  ({ if (e == CHI_EVENT_BEGIN) { RAWSTR("<?xml version=\"1.0\"?>\n<EVENTS>\n"); } CHAR('<'); RAWSTR(chiEventName[e]); })
#define XEVENT_END(e)    ({ if (*xstate) { RAWSTR("/>"); } else { RAWSTR("</"); RAWSTR(chiEventName[e]); CHAR('>'); } CHAR('\n'); if (e == CHI_EVENT_END) { RAWSTR("</EVENTS>\n"); } })
#define XNUM(n)          NUM(n)
#define XSNUM(n)         SNUM(n)
#define XENUM(e,n)       ({ CHAR('"'); RAWSTR(enum##e[n]); CHAR('"'); })

static CHI_WU bool xmlEscape(Log* log, ChiStringRef s) {
    for (size_t i = 0; i < s.size; ++i) {
        char c = (char)s.bytes[i];
        switch (c) {
        case '"':  RAWSTR("&#34;"); break;
        case '\n': RAWSTR("&#10;"); break;
        case '<':  RAWSTR("&lt;");  break;
        case '>':  RAWSTR("&gt;");  break;
        default:   CHAR(c); break;
        }
    }
    return true;
}
