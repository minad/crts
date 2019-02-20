#pragma once

#include <chili/object/string.h>
#include <stdio.h>

typedef struct {
    char*    name;
    uint32_t size, start;
} ZipFile;

typedef struct {
    size_t used, capacity;
    ZipFile* entry;
} ZipFileHash;

typedef struct {
    FILE*       fp;
    ZipFileHash fileHash;
} Zip;

#define ZIP_FOREACH_RESULT(RESULT)                              \
    RESULT(OK, "ok")                                            \
        RESULT(ERROR_OPEN,        "open failed")                \
        RESULT(ERROR_READ,        "read failed")                \
        RESULT(ERROR_SEEK,        "seek failed")                \
        RESULT(ERROR_INFLATE,     "inflate failed")             \
        RESULT(ERROR_NAME,        "invalid name")               \
        RESULT(ERROR_SIZE,        "file too large")             \
        RESULT(ERROR_COMPRESSION, "invalid compression method") \
        RESULT(ERROR_SIGNATURE,   "invalid signature")          \
        RESULT(ERROR_DUPLICATE,   "duplicate file")             \
        RESULT(ERROR_FLAGS,       "unsupported flags")
#define _ZIP_RESULT(N, n) ZIP_##N,
typedef enum { ZIP_FOREACH_RESULT(_ZIP_RESULT) } ZipResult;
#undef _ZIP_RESULT

extern const char* const zipResultName[];
CHI_WU Zip* zipOpen(const char*, ZipResult*);
void zipClose(Zip*);
CHI_WU ZipFile* zipFind(Zip*, ChiStringRef);
CHI_WU ZipResult zipRead(Zip*, ZipFile*, void*);

CHI_DEFINE_AUTO(Zip*, zipClose)
