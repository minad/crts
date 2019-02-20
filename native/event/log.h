#pragma once

#include "../private.h"
#include "../num.h"

typedef struct {
    uint8_t *ptr, *end, buf[0];
} Log;

CHI_INL void logInit(Log* l, size_t size) {
    l->ptr = l->buf;
    l->end = l->buf + size;
}

CHI_INL CHI_WU bool logCheck(Log* l, size_t n) {
    return CHI_LIKELY(l->ptr + n <= l->end);
}

CHI_INL CHI_WU bool logAppend(Log* l, const void* s, size_t n) {
    if (!logCheck(l, n))
        return false;
    memcpy(l->ptr, s, n);
    l->ptr += n;
    return true;
}

CHI_INL CHI_WU bool logString(Log* l, const char* s) {
    return logAppend(l, s, strlen(s));
}

CHI_INL CHI_WU bool logUNumber(Log* l, uint64_t n) {
    if (!logCheck(l, CHI_INT_BUFSIZE))
        return false;
    l->ptr = (uint8_t*)chiShowUInt((char*)l->ptr, n);
    return true;
}

CHI_INL CHI_WU bool logSNumber(Log* l, int64_t n) {
    if (!logCheck(l, CHI_INT_BUFSIZE))
        return false;
    l->ptr = (uint8_t*)chiShowInt((char*)l->ptr, n);
    return true;
}

CHI_INL CHI_WU bool logChar(Log* l, char c) {
    return logAppend(l, &c, 1);
}
