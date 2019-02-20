char* d = (char*)dst;
const char* s = (const char*)src;

while (!WALIGNED(d) && n) {
    *d++ = *s++;
    --n;
}

if (WALIGNED(s)) {
    while (n >= 4 * WSIZE) {
        word_t* dw = (word_t*)(void*)d;
        const word_t* sw = (const word_t*)(const void*)s;
        dw[0] = sw[0]; dw[1] = sw[1]; dw[2] = sw[2]; dw[3] = sw[3];
        d += 4 * WSIZE;
        s += 4 * WSIZE;
        n -= 4 * WSIZE;
    }

    while (n >= WSIZE) {
        word_t* dw = (word_t*)(void*)d;
        const word_t* sw = (const word_t*)(const void*)s;
        dw[0] = sw[0];
        d += WSIZE;
        s += WSIZE;
        n -= WSIZE;
    }
}

while (n) {
    *d++ = *s++;
    --n;
}

return dst;
