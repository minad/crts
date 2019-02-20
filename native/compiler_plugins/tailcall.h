#ifdef __clang__

// noreturn cannot be used, doesnt work with opt passes simplifycfg, latesimplifycfg, globalopt, prune-eh, jump-threading
#define __tailcallable__ __attribute__ ((annotate("__tailcallable__"))) int

#define __tailcall__(f, ...)                                            \
    ({                                                                  \
        __builtin_annotation((f)(__VA_ARGS__), "__tailcall__");         \
        __builtin_unreachable();                                        \
    })

#else

// noreturn cannot be used, void return type doesn't work -> gcc compiler error
#define __tailcallable__ __attribute__ ((tailcallable)) int

#define __tailcall__(f, ...)                            \
    ({                                                  \
        __attribute__ ((unused)) int __tailcall__;      \
        return (f)(__VA_ARGS__);                        \
    })

#endif
