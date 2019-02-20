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

CHI_INL const char* strsplit(const char** rest, char sep, const char** end) {
    for (;;) {
        const char* tok = *rest, *p;
        if (!tok || !*tok)
            return 0;
        if ((p = strchr(tok, sep))) {
            *rest = p + 1;
            *end = p;
        } else {
            *rest = 0;
            *end = tok + strlen(tok);
        }
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
