CHI_EXPORT void CHI_PRIVATE(chiDebugMsg)(const char*, uint32_t, const char*, const char*, ...) CHI_FMT(4, 5);
CHI_EXPORT _Noreturn void CHI_PRIVATE(chiDebugBug)(const char*, uint32_t, const char*, const char*, ...) CHI_FMT(4, 5);
CHI_EXPORT _Noreturn void CHI_PRIVATE(chiDebugAssert)(const char*, uint32_t, const char*, const char*);

#ifdef NDEBUG
#  define CHI_ASSERT(cond)     ({})
#  define CHI_ASSUME(cond)     ((cond) ? ({}) : CHI_UNREACHABLE)
#  define CHI_BUG(...)         __builtin_unreachable()
#  define CHI_DBGMSG(...)      ({})
#else
#  define CHI_ASSUME(cond)     CHI_ASSERT(cond)
#  define CHI_BUG(msg, ...)    _chiDebugBug(__FILE__, __LINE__, __func__, (msg), ##__VA_ARGS__)
#  define CHI_DBGMSG(msg, ...) _chiDebugMsg(__FILE__, __LINE__, __func__, (msg), ##__VA_ARGS__)
#  define CHI_ASSERT(cond)     (CHI_LIKELY(cond) ? ({}) : _chiDebugAssert(__FILE__, __LINE__, __func__, #cond))
#endif

#define CHI_UNREACHABLE CHI_BUG("Unreachable code reached")

/**
 * Assert reference value or empty value.
 *
 * Since empty values are stored unboxed,
 * the check allows false positives.
 * But this should be sufficient to catch bugs.
 * Empty values are stored unboxed to allow
 * a fast reference check, i.e., !chiUnboxed(c).
 *
 * Before moving to unboxed empty values,
 * boxed empty values have been used with
 * size=0 address=0, but this made the reference
 * check more expensive, !chiUnboxed(c) && !chiEmpty(c).
 */
#define CHI_ASSERT_TAGGED(c) \
    CHI_ASSERT(!_chiUnboxed(c) || (CHILI_UN(c) & CHI_BF_SHIFTED_MASK(_CHILI_BF_REFTAG)) == _CHILI_EMPTY_REFTAG);
