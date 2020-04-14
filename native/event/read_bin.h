#define SPAYLOAD_BEGIN(n, p) static CHI_WU const uint8_t* readBin##n(const uint8_t* src, const uint8_t* end, p) {
#define SPAYLOAD_END         return src; }
#define SEVENT_BEGIN                                                    \
    static CHI_WU const uint8_t* readBinEvent(const uint8_t* src, const uint8_t* end, Event* e) { \
    {                                                                   \
        uint8_t _x; READ_BYTE(&_x);                                     \
        if (_x >= _CHI_EVENT_COUNT) return 0;                           \
        e->type = (ChiEvent)_x;                                         \
    }
#define SEVENT_END       return src; }
#define SPAYLOAD(n, p)   src = readBin##n(src, end, CHI_CONST_CAST(p, ChiEvent##n*))
#define SSTR(x)          ({ uint64_t _n; READ_LEB128(&_n); READ_BYTES(&(x).bytes, _n); (x).size = (uint32_t)_n; })
#define SBYTES(x)        ({ uint64_t _n; READ_LEB128(&_n); READ_BYTES(&(x).bytes, _n); (x).size = (uint32_t)_n; })
#define SFIELD(k, v)     v
#define SBLOCK_BEGIN(n)  ({})
#define SBLOCK_END(n)    ({})
#define SDEC(n)          ({ uint64_t _x; READ_LEB128(&_x); n = (typeof(n))_x; })
#define SHEX(n)          SDEC(n)
#define SBOOL(n)         ({ uint8_t _x; READ_BYTE(&_x); n = !!_x; })
#define SENUM(N,n)       ({ uint8_t _x; READ_BYTE(&_x); if (_x >= CHI_DIM(enum##N)) return 0; n = (N)_x; })
