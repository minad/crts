#define WPAYLOAD_BEGIN(n, p) static CHI_WU uint8_t* writeBin##n(uint8_t* dst, uint8_t* end, const p) {
#define WPAYLOAD_END           return dst; }
#define WEVENT_BEGIN     static CHI_WU uint8_t* writeBinEvent(uint8_t* dst, uint8_t* end, const Event* e) { WENUM(ChiEvent,e->type);
#define WEVENT_END       return dst; }
#define WPAYLOAD(n, p)   dst = writeBin##n(dst, end, (p))
#define WSTR(x)          ({ ChiStringRef _x = (x); WRITE_LEB128(_x.size); WRITE_RAWSTR(_x); })
#define WBYTES(x)        ({ ChiBytesRef _x = (x); WRITE_LEB128(_x.size); WRITE_RAWBYTES(_x); })
#define WFIELD(k, v)     v
#define WBLOCK_BEGIN(n)  ({})
#define WBLOCK_END(n)    ({})
#define WDEC(n)          WRITE_LEB128(n)
#define WHEX(n)          WDEC(n)
#define WBOOL(n)         WRITE_BYTE(n)
#define WENUM(_,n)       WRITE_BYTE(n)
