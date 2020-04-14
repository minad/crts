#define SPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, Csv##n)(uint8_t* dst, const p) {
#define SPAYLOAD_END     return dst; }
#define SEVENT_BEGIN     static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, CsvEvent)(uint8_t* dst, const Event* e) { SENUM(ChiEvent, e->type);
#define SEVENT_END       WRITE_CHAR('\n'); return dst; }
#define SPAYLOAD(n, p)   dst = CHI_CAT(WRITE_MODE, Csv##n)(dst, (p))
#define SSTR(x)          ({ WRITE_CHAR('"'); WRITE_ESCSTR(x); WRITE_CHAR('"'); })
#define SBYTES(x)        ({ WRITE_CHAR('"'); WRITE_ESCBYTES(x); WRITE_CHAR('"'); })
#define SBLOCK_BEGIN(n)  ({})
#define SFIELD(k, v)     ({ WRITE_CHAR(','); v; })
#define SBLOCK_END(n)    ({})
#define SDEC(n)          WRITE_DEC(n)
#define SHEX(n)          SDEC(n)
#define SBOOL(n)         WRITE_DEC(n)
#define SENUM(_,n)       WRITE_DEC(n)
