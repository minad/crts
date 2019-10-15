#pragma once

#include <stddef.h>

size_t strlen(const char*);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);
char* strchr(const char*, int);
char* strrchr(const char*, int);
void* memset(void*, int, size_t);
void* memcpy(void* restrict, const void* restrict, size_t);
void* memccpy(void* restrict, const void* restrict, int, size_t);
void* memmove(void*, const void*, size_t);
int memcmp(const void*, const void*, size_t);
void* memchr(const void*, int, size_t);
void* memrchr(const void*, int, size_t);
