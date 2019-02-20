#pragma once

#include <stddef.h>

size_t strlen(const char*);
void* memset(void*, int, size_t);
void* memcpy(void* restrict, const void* restrict, size_t);
void* memmove(void*, const void*, size_t);
int memcmp(const void*, const void*, size_t);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);
char* strchr(const char*, int);
char* strrchr(const char*, int);
void* memchr(const void*, int, size_t);
void* memrchr(const void*, int, size_t);

static inline size_t __intrinsic_strlen(const char* s) { return __builtin_strlen(s); }
static inline void* __intrinsic_memset(void* s, int c, size_t n) { return __builtin_memset(s, c, n); }
static inline void* __intrinsic_memcpy(void* restrict d, const void* restrict s, size_t n) { return __builtin_memcpy(d, s, n); }
static inline void* __intrinsic_memmove(void* d, const void* s, size_t n) { return __builtin_memmove(d, s, n); }
static inline int __intrinsic_memcmp(const void* a, const void* b, size_t n) { return __builtin_memcmp(a, b, n); }
static inline int __intrinsic_strcmp(const char* a, const char* b) { return __builtin_strcmp(a, b); }
static inline int __intrinsic_strncmp(const char* a, const char* b, size_t n) { return __builtin_strncmp(a, b, n); }
static inline char* __intrinsic_strchr(const char* s, int c) { return __builtin_strchr(s, c); }
static inline char* __intrinsic_strrchr(const char* s, int c) { return __builtin_strrchr(s, c); }
static inline void* __intrinsic_memchr(const void* s, int c, size_t n) { return __builtin_memchr(s, c, n); }
static inline void* __intrinsic_memrchr(const void* s, int c, size_t n) { return memrchr(s, c, n); }

#include "bits/string_def.h"
