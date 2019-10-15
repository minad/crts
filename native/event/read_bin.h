#define WPAYLOAD_BEGIN(n, p) static CHI_WU const uint8_t* readBin##n(const uint8_t* src, const uint8_t* end, p) {
#define WPAYLOAD_END         return src; }
#define WEVENT_BEGIN                                                    \
    static CHI_WU const uint8_t* readBinEvent(const uint8_t* src, const uint8_t* end, Event* e) { \
    {                                                                   \
        uint8_t _x; READ_BYTE(&_x);                                     \
        if (_x >= _CHI_EVENT_COUNT) return 0;                           \
        e->type = (ChiEvent)_x;                                         \
    }
#define WEVENT_END       return src; }
#define WPAYLOAD(n, p)   src = readBin##n(src, end, CHI_CONST_CAST(p, ChiEvent##n*))
#define WSTR(x)          ({ uint64_t _n; READ_LEB128(&_n); READ_BYTES(&(x).bytes, _n); (x).size = (uint32_t)_n; })
#define WBYTES(x)        ({ uint64_t _n; READ_LEB128(&_n); READ_BYTES(&(x).bytes, _n); (x).size = (uint32_t)_n; })
#define WFIELD(k, v)     v
#define WBLOCK_BEGIN(n)  ({})
#define WBLOCK_END(n)    ({})
#define WDEC(n)          ({ uint64_t _x; READ_LEB128(&_x); n = (typeof(n))_x; })
#define WHEX(n)          WDEC(n)
#define WBOOL(n)         ({ uint8_t _x; READ_BYTE(&_x); n = !!_x; })
#define WENUM(N,n)       ({ uint8_t _x; READ_BYTE(&_x); if (_x >= CHI_DIM(enum##N)) return 0; n = (N)_x; })
