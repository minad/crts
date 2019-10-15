#define WPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* writeCsv##n(uint8_t* dst, uint8_t* end, const p) {
#define WPAYLOAD_END     return dst; }
#define WEVENT_BEGIN     static CHI_WU uint8_t* writeCsvEvent(uint8_t* dst, uint8_t* end, const Event* e) { WENUM(ChiEvent, e->type);
#define WEVENT_END       if (dst) WRITE_CHAR('\n'); return dst; }
#define WPAYLOAD(n, p)   dst = writeCsv##n(dst, end, (p))
#define WSTR(x)          ({ WRITE_CHAR('"'); WRITE_ESCAPESTR(x); WRITE_CHAR('"'); })
#define WBYTES(x)        ({ WRITE_CHAR('"'); WRITE_ESCAPEBYTES(x); WRITE_CHAR('"'); })
#define WBLOCK_BEGIN(n)  ({})
#define WFIELD(k, v)     ({ WRITE_CHAR(','); v; })
#define WBLOCK_END(n)    ({})
#define WDEC(n)          WRITE_DEC(n)
#define WHEX(n)          WDEC(n)
#define WBOOL(n)         WRITE_DEC(n)
#define WENUM(_,n)       WRITE_DEC(n)
