#include "mem.h"
#include "utf16.h"

size_t chiUtf8Size(const uint16_t* w) {
    size_t len = 0;
    while (*w) {
        uint32_t c;
        if (*w >= 0xD800 && *w <= 0xDBFF) {
            c = ((w[0] - 0xD800U) << 10) + (w[1] - 0xDC00U) + 0x10000U;
            w += 2;
        } else {
            c = *w++;
        }
        len += chiUtf8EncodeSize(c);
    }
    return len + 1;
}

void chiUtf16To8(const uint16_t* w, char* s, size_t n) {
    CHI_ASSERT(n > 0);
    char* ends = s + n;
    while (*w) {
        uint32_t c;
        if (*w >= 0xD800 && *w <= 0xDBFF) {
            c = ((w[0] - 0xD800U) << 10) + (w[1] - 0xDC00U) + 0x10000U;
            w += 2;
        } else {
            c = *w++;
        }
        if (s + chiUtf8EncodeSize(c) >= ends)
            break;
        s += chiUtf8Encode(c, (uint8_t*)s);
    }
    *s = 0;
}

size_t chiUtf16Size(const char* s) {
    size_t len = 0;
    const char* ends = s + strlen(s);
    while (s < ends) {
        uint32_t c = chiUtf8Decode((const uint8_t**)&s, (const uint8_t*)ends);
        len += 1U + (c >= 0x10000);
    }
    return len + 1;
}

void chiUtf8To16(const char* s, uint16_t* w, size_t n) {
    CHI_ASSERT(n > 0);
    uint16_t* endw = w + n;
    const char* ends = s + strlen(s);
    while (s < ends) {
        uint32_t c = chiUtf8Decode((const uint8_t**)&s, (const uint8_t*)ends);
        if (c < 0x10000) {
            if (w + 1 >= endw)
                break;
            *w++ = (uint16_t)c;
        } else {
            if (w + 2 >= endw)
                break;
            c -= 0x10000;
            *w++ = (uint16_t)(0xD800 + (c >> 10));
            *w++ = (uint16_t)(0xDC00 + (c & 0x3FF));
        }
    }
    *w = 0;
}

char* chiAllocUtf16To8(const uint16_t* w) {
    size_t n = chiUtf8Size(w);
    char* s = chiAllocArr(char, n);
    chiUtf16To8(w, s, n);
    return s;
}

uint16_t* chiAllocUtf8To16(const char* s) {
    size_t n = chiUtf16Size(s);
    uint16_t* w = chiAllocArr(uint16_t, n);
    chiUtf8To16(s, w, n);
    return w;
}
