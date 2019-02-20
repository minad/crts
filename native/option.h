#pragma once

#include "private.h"

typedef enum {
    CHI_OPT_END,
    CHI_OPT_TITLE,
    CHI_OPT_FLAG,
    CHI_OPT_SET,
    CHI_OPT_UINT32,
    CHI_OPT_UINT64,
    CHI_OPT_SIZE,
    CHI_OPT_STRING,
    CHI_OPT_CHOICE,
} ChiOptionType;

typedef enum {
    CHI_OPTRESULT_OK,
    CHI_OPTRESULT_EXIT,
    CHI_OPTRESULT_HELP,
    CHI_OPTRESULT_ERROR,
} ChiOptionResult;

#define CHI_OPT_FIELD(ft, f)                                          \
    (CHI_STATIC_FAIL(__builtin_types_compatible_p(CHI_FIELD_TYPE(CHI_OPT_TARGET, f), ft)) + \
     offsetof(CHI_OPT_TARGET, f))
#define CHI_OPT_DESC(t, ...) { .type = CHI_OPT_##t, ##__VA_ARGS__ },
#define CHI_OPT_DESC_TITLE(n) CHI_OPT_DESC(TITLE, .name = CHI_MUST_BE_STRING(n))
#define CHI_OPT_DESC_END      CHI_OPT_DESC(END)
#define CHI_OPT_DESC_FLAG(f, n, d)                                      \
    CHI_OPT_DESC(FLAG, .field = CHI_OPT_FIELD(bool, f), .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_DESC_UINT32(f, a, b, n, d)                              \
    CHI_OPT_DESC(UINT32, .field = CHI_OPT_FIELD(uint32_t, f), .uint32Range = { a, b }, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_DESC_CHOICE(f, n, d, c)                                 \
    CHI_OPT_DESC(CHOICE, .field = CHI_OPT_FIELD(uint32_t, f), .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d), .choice = CHI_MUST_BE_STRING(c))
#define CHI_OPT_DESC_UINT64(f, a, b, n, d)                              \
    CHI_OPT_DESC(UINT64, .field = CHI_OPT_FIELD(uint64_t, f), .uint64Range = { a, b }, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_DESC_SIZE(f, a, b, n, d)                                \
    CHI_OPT_DESC(SIZE, .field = CHI_OPT_FIELD(size_t, f), .sizeRange = { a, b }, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_DESC_STRING(f, n, d)                                    \
    CHI_OPT_DESC(STRING, .field = CHI_OPT_FIELD(char[], f), .string.size = CHI_FIELD_SIZE(CHI_OPT_TARGET, f), .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_DESC_CB(t, f, n, d) CHI_OPT_DESC(t, .cb = true, .cbFn = f, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))

typedef struct ChiSink_ ChiSink;
typedef const struct ChiOption_ ChiOption;
typedef const struct ChiOptionList_ ChiOptionList;
typedef const struct ChiOptionParser_ ChiOptionParser;
typedef ChiOptionResult (*ChiOptionCallback)(const ChiOptionParser*, const ChiOptionList*, const ChiOption*, const void*);

struct CHI_PACKED ChiOption_ {
    const char        *name;      ///< Option name and description
    ChiOptionType     type : 7;  ///< Option value type
    bool              cb : 1;
    union {
        size_t            field;     ///< Offset of the field within target
        ChiOptionCallback cbFn;
    };
    union {
        const char*                           choice;      ///< CHI_OPT_CHOICE,  List of choices separated by comma
        struct { size_t size;               } string;      ///< CHI_OPT_STRING
        struct { uint64_t val; size_t size; } set;         ///< CHI_OPT_SET
        struct { uint32_t min, max;         } uint32Range; ///< CHI_OPT_UINT32
        struct { uint64_t min, max;         } uint64Range; ///< CHI_OPT_UINT64
        struct { size_t   min, max;         } sizeRange;   ///< CHI_OPT_SIZE
    };
};

struct ChiOptionList_ {
    const ChiOption* desc;
    void* target;
};

struct ChiOptionParser_ {
    ChiSink *out, *err;
    const ChiOptionList* list;
};

#if CHI_OPTION_ENABLED
void chiOptionHelp(const ChiOptionParser*);
CHI_WU ChiOptionResult chiOptionArgs(const ChiOptionParser*, int*, char**);
CHI_WU ChiOptionResult chiOptionEnv(const ChiOptionParser*, const char*);
#else
void chiOptionHelp(const ChiOptionParser* CHI_UNUSED(p)) {}
CHI_INL CHI_WU ChiOptionResult chiOptionArgs(const ChiOptionParser* CHI_UNUSED(p), int* CHI_UNUSED(argc), char** CHI_UNUSED(argv)) {
    return CHI_OPTRESULT_OK;
}
CHI_INL CHI_WU ChiOptionResult chiOptionEnv(const ChiOptionParser* CHI_UNUSED(p), const char* CHI_UNUSED(var)) {
    return CHI_OPTRESULT_OK;
}
#endif
