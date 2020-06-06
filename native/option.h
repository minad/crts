#pragma once

#include "private.h"

typedef enum {
    CHI_OPTTYPE_FLAG,
    CHI_OPTTYPE_UINT32,
    CHI_OPTTYPE_UINT64,
    CHI_OPTTYPE_SIZE,
    CHI_OPTTYPE_STRING,
    CHI_OPTTYPE_CHOICE,
} ChiOptionType;

typedef enum {
    CHI_OPTRESULT_OK,
    CHI_OPTRESULT_EXIT,
    CHI_OPTRESULT_HELP,
    CHI_OPTRESULT_ERROR,
} ChiOptionResult;

#define CHI_OPT_GROUP(n, t, ...) ChiOptionGroup n = { .title = t, .options = (ChiOption[]){__VA_ARGS__}, .count = sizeof((ChiOption[]){__VA_ARGS__})/sizeof(ChiOption) };
#define CHI_OPT_FIELD(ft, f)                                          \
    (CHI_STATIC_FAIL(__builtin_types_compatible_p(CHI_FIELD_TYPE(CHI_OPT_STRUCT, f), ft)) + \
     offsetof(CHI_OPT_STRUCT, f))
#define _CHI_OPT(t, ...) { .type = CHI_OPTTYPE_##t, ##__VA_ARGS__ },
#define CHI_OPT_FLAG(f, n, d)                                      \
    _CHI_OPT(FLAG, .field = CHI_OPT_FIELD(bool, f), .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_UINT32(f, a, b, n, d)                              \
    _CHI_OPT(UINT32, .field = CHI_OPT_FIELD(uint32_t, f), .uint32Range = { a, b }, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_CHOICE(f, n, d, c)                                 \
    _CHI_OPT(CHOICE, .field = CHI_OPT_FIELD(uint32_t, f), .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d), .choice = CHI_MUST_BE_STRING(c))
#define CHI_OPT_UINT64(f, a, b, n, d)                              \
    _CHI_OPT(UINT64, .field = CHI_OPT_FIELD(uint64_t, f), .uint64Range = { a, b }, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_SIZE(f, a, b, n, d)                                \
    _CHI_OPT(SIZE, .field = CHI_OPT_FIELD(size_t, f), .sizeRange = { a, b }, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_STRING(f, n, d)                                    \
    _CHI_OPT(STRING, .field = CHI_OPT_FIELD(char[], f), .string.size = CHI_FIELD_SIZE(CHI_OPT_STRUCT, f), .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))
#define CHI_OPT_CB(t, f, n, d) _CHI_OPT(t, .cb = true, .cbFn = f, .name = CHI_MUST_BE_STRING(n) "\0" CHI_MUST_BE_STRING(d))

typedef struct ChiSink_ ChiSink;
typedef const struct ChiOption_ ChiOption;
typedef const struct ChiOptionGroup_ ChiOptionGroup;
typedef const struct ChiOptionAssoc_ ChiOptionAssoc;
typedef const struct ChiOptionParser_ ChiOptionParser;
typedef ChiOptionResult (*ChiOptionCallback)(ChiOptionParser*, ChiOptionAssoc*, ChiOption*, const void*);

struct CHI_PACKED ChiOption_ {
    const char        *name;      ///< Option name and description
    ChiOptionType     type : 7;  ///< Option value type
    bool              cb : 1;
    union {
        size_t            field;     ///< Offset of the field within target
        ChiOptionCallback cbFn;
    };
    union {
        const char*                 choice;        ///< CHI_OPTTYPE_CHOICE,  List of choices separated by comma
        struct { size_t size;       } string;      ///< CHI_OPTTYPE_STRING
        struct { uint32_t min, max; } uint32Range; ///< CHI_OPTTYPE_UINT32
        struct { uint64_t min, max; } uint64Range; ///< CHI_OPTTYPE_UINT64
        struct { size_t   min, max; } sizeRange;   ///< CHI_OPTTYPE_SIZE
    };
};

struct CHI_PACKED ChiOptionGroup_ {
    ChiOption* options;
    uint16_t count;
    char title[];
};

struct ChiOptionAssoc_ {
    ChiOptionGroup* group;
    void* target;
};

struct ChiOptionParser_ {
    ChiSink *help, *usage;
    ChiOptionAssoc* assocs;
};

#if CHI_OPTION_ENABLED
CHI_INTERN void chiOptionHelp(ChiOptionParser*);
CHI_INTERN CHI_WU ChiOptionResult chiOptionArgs(ChiOptionParser*, int*, char**);
CHI_INTERN CHI_WU ChiOptionResult chiOptionEnv(ChiOptionParser*, const char*);
#else
void chiOptionHelp(ChiOptionParser* CHI_UNUSED(p)) {}
CHI_INL CHI_WU ChiOptionResult chiOptionArgs(ChiOptionParser* CHI_UNUSED(p), int* CHI_UNUSED(argc), char** CHI_UNUSED(argv)) {
    return CHI_OPTRESULT_OK;
}
CHI_INL CHI_WU ChiOptionResult chiOptionEnv(ChiOptionParser* CHI_UNUSED(p), const char* CHI_UNUSED(var)) {
    return CHI_OPTRESULT_OK;
}
#endif
