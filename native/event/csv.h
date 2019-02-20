#define XFORMAT          csv
#define XSTR(x)          ({ CHAR('"'); CHECK(csvEscape, (x)); CHAR('"'); })
#define XBLOCK_BEGIN(n)  ({})
#define XUBLOCK_BEGIN(n) ({})
#define XFIELD(k, v)     ({ CHI_NOWARN_UNUSED(xstate); CHAR(','); v; })
#define XBLOCK_END(n)    ({})
#define XUBLOCK_END(n)   ({})
#define XINIT            void* xstate = 0
#define XSTATE           void*
#define XEVENT_BEGIN(e)  NUM(e)
#define XEVENT_END(e)    CHAR('\n')
#define XNUM(n)          NUM(n)
#define XSNUM(n)         SNUM(n)
#define XENUM(_,n)       XNUM(n)

/**
 * Escape CSV strings
 *
 * We don't strictly adhere to the RFC4180 standard.
 *
 * - We are using varying numbers of columns
 * - Line endings are '\n' instead of '\r\n'
 * - Line breaks are escaped as '\n', such that
 *   events do not span multiple lines to ease parsing.
 */
static CHI_WU bool csvEscape(Log* log, ChiStringRef s) {
    for (size_t i = 0; i < s.size; ++i) {
        char c = (char)s.bytes[i];
        switch (c) {
        case '"':  RAWSTR("\"\""); break;
        case '\n': RAWSTR("\\n"); break;
        case '\\': RAWSTR("\\"); break;
        default:   CHAR(c); break;
        }
    }
    return true;
}
