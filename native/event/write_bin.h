#define SPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, Bin##n)(uint8_t* dst, const p) {
#define SPAYLOAD_END           return dst; }
#define SEVENT_BEGIN     static CHI_WU uint8_t* CHI_CAT(WRITE_MODE, BinEvent)(uint8_t* dst, const Event* e) { SENUM(ChiEvent,e->type);
#define SEVENT_END       return dst; }
#define SPAYLOAD(n, p)   dst = CHI_CAT(WRITE_MODE, Bin##n)(dst, (p))
#define SSTR(x)          ({ ChiStringRef _x = (x); WRITE_LEB128(_x.size); WRITE_RAWSTR(_x); })
#define SBYTES(x)        ({ ChiBytesRef _x = (x); WRITE_LEB128(_x.size); WRITE_RAWBYTES(_x); })
#define SFIELD(k, v)     v
#define SBLOCK_BEGIN(n)  ({})
#define SBLOCK_END(n)    ({})
#define SDEC(n)          WRITE_LEB128(n)
#define SHEX(n)          SDEC(n)
#define SBOOL(n)         WRITE_BYTE(n)
#define SENUM(_,n)       WRITE_BYTE(n)
