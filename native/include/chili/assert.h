CHI_API _Noreturn void CHI_PRIVATE(chiAssert)(const char*, uint32_t, const char*, const char*);
CHI_API _Noreturn void CHI_PRIVATE(chiBug)(const char*, uint32_t, const char*, const char*, ...) CHI_FMT(4, 5);

#ifdef NDEBUG
#  define _CHI_ASSERT_OK(c) 1
#  define CHI_BUG(...) __builtin_unreachable()
#else
#  define _CHI_ASSERT_OK(c) CHI_LIKELY(c)
#  define CHI_BUG(msg, ...) _chiBug(__FILE__, __LINE__, __func__, (msg), ##__VA_ARGS__)
#endif

#define CHI_UNREACHABLE CHI_BUG("Unreachable code reached")

#ifdef __clang__
void _chiStaticAssert(bool cond) __attribute__((diagnose_if(!cond, "Static assertion failed", "error")));
#else
void _chiStaticAssert(bool) __attribute__((error("Static assertion failed")));
#endif

// We cannot use the standard assert macro here since it would expand cond.
// Therefore we have our own function _chiAssert.
#define CHI_ASSERT(cond)                                                \
    __builtin_choose_expr(__builtin_constant_p(cond),                   \
                          (cond) ? ({}) : _chiStaticAssert(cond),       \
                          _CHI_ASSERT_OK(cond)                          \
                          ? ({})                                        \
                          : _chiAssert(__FILE__, __LINE__, __func__, #cond))
