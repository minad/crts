#if CHI_LIKELY_STATS_ENABLED
#  define CHI_LIKELY(x)       _chiLikely(!!(x), "LIKELY(" #x ") in " __FILE__ ":" CHI_STRINGIZE(__LINE__))
#  define CHI_UNLIKELY(x)     _chiUnlikely(!!(x), "UNLIKELY(" #x ") in " __FILE__ ":" CHI_STRINGIZE(__LINE__))
#else
#  define CHI_LIKELY(x)       __builtin_expect(!!(x), true)
#  define CHI_UNLIKELY(x)     __builtin_expect(!!(x), false)
#endif

CHI_API bool CHI_PRIVATE(chiLikely)(bool, const char*);
CHI_API bool CHI_PRIVATE(chiUnlikely)(bool, const char*);
