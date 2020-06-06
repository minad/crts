#include <stdlib.h>
#include "color.h"
#include "mem.h"
#include "num.h"
#include "sink.h"
#include "strutil.h"

#if CHI_OPTION_ENABLED

void chiOptionHelp(ChiOptionParser* parser) {
    int max = 0;
    for (ChiOptionAssoc* assoc = parser->assocs; assoc->group; ++assoc) {
        for (ChiOption* opt = assoc->group->options; opt < assoc->group->options + assoc->group->count; ++opt)
            CHI_SETMAX(&max, (int)strlen(opt->name) + (opt->type == CHI_OPTTYPE_FLAG ? 0 : 2));
    }

    for (ChiOptionAssoc* assoc = parser->assocs; assoc->group; ++assoc) {
        chiSinkFmt(parser->help, "%s"TitleBegin"%s OPTIONS"TitleEnd"\n",
                   assoc != parser->assocs ? "\n" : "", assoc->group->title);
        for (ChiOption* opt = assoc->group->options; opt < assoc->group->options + assoc->group->count; ++opt) {
            char cbField[8] = { 0 };
            void* field = opt->cb ? cbField : (char*)assoc->target + opt->field;
            int n = (int)strlen(opt->name) + 2;
            const char* desc = opt->name + strlen(opt->name) + 1;
            switch (opt->type) {
            case CHI_OPTTYPE_FLAG:
                n -= 2;
                chiSinkFmt(parser->help, "  -R%s%*w %s\n", opt->name, max - n, desc);
                break;
            case CHI_OPTTYPE_UINT32:
                chiSinkFmt(parser->help, "  -R%s=n%*w %s (dfl=%u, %u-%u)\n", opt->name, max - n,
                           desc, chiPeekUInt32(field), opt->uint32Range.min, opt->uint32Range.max);
                break;
            case CHI_OPTTYPE_UINT64:
                chiSinkFmt(parser->help, "  -R%s=n%*w %s (dfl=%ju, %ju-%ju)\n", opt->name, max - n,
                           desc, chiPeekUInt64(field), opt->uint64Range.min, opt->uint64Range.max);
                break;
            case CHI_OPTTYPE_SIZE:
                chiSinkFmt(parser->help, "  -R%s=s%*w %s (dfl=%Z, %Z-%Z)\n", opt->name, max - n,
                           desc, chiPeekSize(field), opt->sizeRange.min, opt->sizeRange.max);
                break;
            case CHI_OPTTYPE_STRING:
                {
                    const char* val = (const char*)field;
                    chiSinkFmt(parser->help, "  -R%s=s%*w %s", opt->name, max - n, desc);
                    if (val[0])
                        chiSinkFmt(parser->help, " (dfl=%s)", val);
                    chiSinkPutc(parser->help, '\n');
                }
                break;
            case CHI_OPTTYPE_CHOICE:
                chiSinkFmt(parser->help, "  -R%s=s%*w %s (%s)\n", opt->name, max - n, desc, opt->choice);
                break;
            }
        }
    }
}

CHI_FMT(2, 3)
static CHI_WU ChiOptionResult error(ChiOptionParser* parser, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chiSinkFmtv(parser->usage, fmt, ap);
    va_end(ap);
    chiSinkPuts(parser->usage, "\nTry '-Rhelp' for more information.\n");
    return CHI_OPTRESULT_ERROR;
}

static CHI_WU ChiOptionResult invalidArg(ChiOptionParser* parser, const char* arg) {
    return error(parser, "Invalid argument '%s'.", arg);
}

static CHI_WU ChiOptionResult parseInt(ChiOptionParser* parser, const char* arg, const char* val,
                                       uint64_t min, uint64_t max, uint64_t* res) {
    const char* end = val;
    uint64_t ival;
    if (!chiReadUInt64(&ival, &end) || *end)
        return invalidArg(parser, arg);
    if (ival < min || ival > max)
        return error(parser, "Argument '%s' out of range %ju-%ju.", arg, min, max);
    *res = ival;
    return CHI_OPTRESULT_OK;
}

static CHI_WU ChiOptionResult parseChoice(ChiOptionParser* parser, const char* arg,
                                          const char* val, const char* choices, uint32_t* res) {
    const char* c = choices;
    for (uint32_t i = 0; ; ++i) {
        const char *end, *tok = strsplit(&c, ',', &end);
        if (!tok)
            return error(parser, "Invalid choice '%s'. Valid choices are %s.", arg, choices);
        tok = strskip(tok, ' ');
        size_t tokSize = (size_t)(end - tok);
        if (memeqstr(tok, tokSize, val)) {
            *res = i;
            return CHI_OPTRESULT_OK;
        }
    }
}

static CHI_WU ChiOptionResult parseSize(ChiOptionParser* parser, const char* arg, const char* val,
                                        size_t min, size_t max, size_t* res) {
    const char* end = val;
    size_t factor = 1;
    uint64_t ival;
    if (!chiReadUInt64(&ival, &end))
        return invalidArg(parser, arg);
    if (!end[1] && (end[0] == 'k' || end[0] == 'K'))
        factor = CHI_KiB(1);
    else if (!end[1] && (end[0] == 'm' || end[0] == 'M'))
        factor = CHI_MiB(1);
    else if (!end[1] && (end[0] == 'g' || end[0] == 'G'))
        factor = CHI_GiB(1);
    else if (end[0])
        return error(parser, "Argument '%s' has invalid size suffix.", val);
    size_t size = (size_t)ival;
    if (size != ival
        || __builtin_mul_overflow(size, factor, &size)
        || size < min || size > max)
        return error(parser, "Argument '%s' out of range %Z-%Z.", arg, min, max);
    *res = size;
    return CHI_OPTRESULT_OK;
}

static CHI_WU ChiOptionResult parseOption(ChiOptionParser* parser, const char* arg) {
    const char *end = strchrnul(arg, '='), *val = 0;
    size_t nameSize = (size_t)(end - arg);
    if (*end == '=') {
        val = end + 1;
        if (!*val)
            return error(parser, "Invalid empty argument '%s'.", arg);
    }

    if (!val && memeqstr(arg, nameSize, "help"))
        return CHI_OPTRESULT_HELP;

    for (ChiOptionAssoc* assoc = parser->assocs; assoc->group; ++assoc) {
        for (ChiOption* opt = assoc->group->options; opt < assoc->group->options + assoc->group->count; ++opt) {
            if (!memeqstr(arg, nameSize, opt->name))
                continue;
            char cbField[8] = { 0 };
            void* field = opt->cb ? cbField : (char*)assoc->target + opt->field;
            if (!val) {
                if (opt->type == CHI_OPTTYPE_FLAG)
                    *((bool*)field) = true;
                else
                    return error(parser, "Argument '%s' requires a value.", arg);
            } else {
                switch (opt->type) {
                case CHI_OPTTYPE_FLAG:
                    if (streq(val, "t"))
                        *((bool*)field) = true;
                    else if (streq(val, "f"))
                        *((bool*)field) = false;
                    else
                        return invalidArg(parser, arg);
                    break;
                case CHI_OPTTYPE_UINT32:
                    {
                        uint64_t x = 0;
                        ChiOptionResult res = parseInt(parser, arg, val,
                                                       opt->uint32Range.min, opt->uint32Range.max, &x);
                        if (res != CHI_OPTRESULT_OK)
                            return res;
                        chiPokeUInt32(field, (uint32_t)x);
                    }
                    break;
                case CHI_OPTTYPE_UINT64:
                    {
                        uint64_t x = 0;
                        ChiOptionResult res = parseInt(parser, arg, val,
                                                       opt->uint64Range.min, opt->uint64Range.max, &x);
                        if (res != CHI_OPTRESULT_OK)
                            return res;
                        chiPokeUInt64(field, x);
                    }
                    break;
                case CHI_OPTTYPE_SIZE:
                    {
                        size_t x = 0;
                        ChiOptionResult res = parseSize(parser, arg, val,
                                                        opt->sizeRange.min, opt->sizeRange.max, &x);
                        if (res != CHI_OPTRESULT_OK)
                            return res;
                        chiPokeSize(field, x);
                    }
                    break;
                case CHI_OPTTYPE_STRING:
                    if (!opt->cb) {
                        size_t n = strlen(val);
                        if (n + 1 > opt->string.size)
                            return error(parser, "Argument '%s' is too long.", arg);
                        memcpy(field, val, n + 1);
                    }
                    break;
                case CHI_OPTTYPE_CHOICE:
                    {
                        uint32_t x = 0;
                        ChiOptionResult res = parseChoice(parser, arg, val, opt->choice, &x);
                        if (res != CHI_OPTRESULT_OK)
                            return res;
                        chiPokeUInt32(field, x);
                    }
                    break;
                }
            }
            return opt->cb ? opt->cbFn(parser, assoc, opt, opt->type == CHI_OPTTYPE_STRING ? val : cbField) : CHI_OPTRESULT_OK;
        }
    }

    return error(parser, "Invalid runtime option '%s'.", arg);
}

ChiOptionResult chiOptionArgs(ChiOptionParser* parser, int* argc, char** argv) {
    int n = 0;
    for (int i = 0; i < *argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] == 'R') {
            ChiOptionResult res = parseOption(parser, argv[i] + 2);
            if (res != CHI_OPTRESULT_OK)
                return res;
        } else {
            argv[n++] = argv[i];
        }
    }
    *argc = n;
    argv[*argc] = 0;
    return CHI_OPTRESULT_OK;
}

ChiOptionResult chiOptionEnv(ChiOptionParser* parser, const char* var) {
    CHI_AUTO_FREE(val, chiGetEnv(var));
    if (!val)
        return CHI_OPTRESULT_OK;

    char* tok, *rest = val;
    while ((tok = strsplitmut(&rest, ' '))) {
        if (tok[0] != '-' || tok[1] != 'R')
            return error(parser, "Invalid argument '%s' in environment variable '%s'.", tok, var);
        ChiOptionResult res = parseOption(parser, tok + 2);
        if (res != CHI_OPTRESULT_OK)
            return res;
    }

    return CHI_OPTRESULT_OK;
}

#endif
