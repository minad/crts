#pragma once

#include "private.h"

CHI_INL bool memeq(const void* a, const void* b, size_t n) {
    return !memcmp(a, b, n);
}

CHI_INL bool memends(const void* s, size_t n, const char* post) {
    size_t m = strlen(post);
    return n >= m && memeq((const char*)s + n - m, post, m);
}

CHI_INL bool memeqstr(const void* a, size_t n, const char* b) {
    return n == strlen(b) && memeq(a, b, n);
}

CHI_INL bool streq(const char* a, const char* b) {
    return !strcmp(a, b);
}

CHI_INL bool strstarts(const char* s, const char* pre) {
    return !strncmp(s, pre, strlen(pre));
}

CHI_INL bool strends(const char* s, const char* post) {
    return memends(s, strlen(s), post);
}

CHI_INL const char* strskip(const char* s, char c) {
    while (*s == c)
        ++s;
    return s;
}

#ifndef __GLIBC__
CHI_INL char* _strchrnul(const char* s, int c) {
    while (*s && *s != c)
        ++s;
    return CHI_CONST_CAST(s, char*);
}
#define strchrnul _strchrnul
#endif

CHI_INL const char* strsplit(const char** rest, char sep, const char** end) {
    for (;;) {
        const char* tok = *rest;
        if (!tok || !*tok)
            return 0;
        *end = strchrnul(tok, sep);
        *rest = **end == sep ? *end + 1 : 0;
        if (*end != tok)
            return tok;
    }
}

CHI_INL char* strsplitmut(char** rest, char sep) {
    char *end, *tok = CHI_CONST_CAST(strsplit(CHI_CAST(rest, const char**), sep, CHI_CAST(&end, const char**)), char*);
    if (tok)
        *end = 0;
    return tok;
}
