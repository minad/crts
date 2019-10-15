#include <chili.h>

#ifndef CHI_INTERN
#  define CHI_INTERN
#endif

#ifndef CHI_EXTERN
#  define CHI_EXTERN extern
#endif

#define CHI_AUTO(n, i, f)     typeof (i) n __attribute__ ((cleanup(chi_auto_##f))) = (i)
#define CHI_DEFINE_AUTO(t, f) CHI_INL void chi_auto_##f(t* p) { if (*p) f(*p); }

#define CHI_DEFINE_AUTO_LOCK(t, lock, unlock)           \
    CHI_NEWTYPE(Lock##t, t*)                            \
        CHI_INL ChiLock##t _chiLock##t(t* x) {          \
        lock(x);                                        \
        return CHI_WRAP(Lock##t, x);                    \
    }                                                   \
    CHI_INL void chi_auto_unlock##t(ChiLock##t* l) {    \
        unlock(CHI_UN(Lock##t, *l));                    \
    }

#define _CHI_AUTO_LOCK(l, t, x) CHI_AUTO(l, _chiLock##t(x), unlock##t); CHI_NOWARN_UNUSED(l);
#define CHI_AUTO_LOCK(t, m)     _CHI_AUTO_LOCK(CHI_GENSYM, t, m)

#define _CHI_OUTER(fp, type, field, ptr)                                \
    ({                                                                  \
        const CHI_FIELD_TYPE(type, field)* fp = (ptr);                  \
        CHI_CAST(((const char*)fp - offsetof(type, field)), type*);     \
    })
#define CHI_OUTER(type, field, ptr) _CHI_OUTER(CHI_GENSYM, type, field, ptr)

#define CHI_FIELD_SIZE(s, f)  (sizeof (((s*)0)->f))
#define CHI_FIELD_TYPE(s, f)  typeof (((s*)0)->f)

#define _CHI_SWAP(v, x, y)    ({ typeof(x) v = (x); (x) = (y); (y) = v; ({}); })
#define CHI_SWAP(x, y)        _CHI_SWAP(CHI_GENSYM, x, y)

#define _CHI_IGNORE_RESULT(r, x) ({ typeof(x) r = x; CHI_NOWARN_UNUSED(r); })
#define CHI_IGNORE_RESULT(x)     _CHI_IGNORE_RESULT(CHI_GENSYM, x)

#define CHI_KiB(s)            ((s) * UINT64_C(1024))
#define CHI_MiB(s)            (UINT64_C(1024) * CHI_KiB(s))
#define CHI_GiB(s)            (UINT64_C(1024) * CHI_MiB(s))
#define CHI_TiB(s)            (UINT64_C(1024) * CHI_GiB(s))

#define CHI_ZERO_STRUCT(p)    memset((p), 0, CHI_SIZEOF_STRUCT(p))
#define CHI_ZERO_ARRAY(p)     memset((p), 0, CHI_SIZEOF_ARRAY(p))

#define CHI_DIV_ROUNDUP(i, r) (((i) + (r) - 1) / (r))
#define CHI_ROUNDUP(i, r)     (CHI_DIV_ROUNDUP((i), (r)) * (r))

#define CHI_ALIGNUP(i, a)     (((i) + (a) - 1) & CHI_ALIGNMASK(a))
#define CHI_ALIGNMASK(a)      (~((a) - 1))

#define CHI_BYTES_TO_WORDS(i) CHI_DIV_ROUNDUP((i), CHI_WORDSIZE)
#define CHI_SIZEOF_WORDS(x)   CHI_BYTES_TO_WORDS(sizeof (x))

#define _CHI_ONCE(mutex, init, ...)                                     \
    ({                                                                  \
        static ChiMutex mutex = CHI_MUTEX_STATIC_INIT;                  \
        static bool init = false;                                       \
        CHI_LOCK_MUTEX(&mutex);                                         \
        if (!init) {                                                    \
            init = true;                                                \
            ({ __VA_ARGS__ });                                          \
        }                                                               \
    })
#define CHI_ONCE(...) _CHI_ONCE(CHI_GENSYM, CHI_GENSYM, __VA_ARGS__)

#ifdef NDEBUG
#  define CHI_ASSERT_ONCE ({})
#else
#  define _CHI_ASSERT_ONCE(once, old)                                   \
    ({                                                                  \
        static _Atomic(bool) once = false;                              \
        CHI_ASSERT(!atomic_exchange_explicit(&once, true, memory_order_relaxed)); \
    })
#define CHI_ASSERT_ONCE _CHI_ASSERT_ONCE(CHI_GENSYM, CHI_GENSYM)
#endif

#define CHI_UNITTYPE(n)     \
    CHI_WARN_OFF(c++-compat) \
        typedef struct {} n; \
    CHI_WARN_ON

#define CHI_CONST_CAST(x, t)                                            \
    ({                                                                  \
        _Static_assert(__builtin_types_compatible_p(const t, typeof(x)), "invalid const cast"); \
        (t)(uintptr_t)(x);                                              \
    })

#define _CHI_ALIGN_CAST(v, x, t)                                        \
    ({                                                                  \
        uint8_t* v = (x);                                               \
        CHI_ASSERT(((uintptr_t)v) % _Alignof(typeof(*v)) == 0);         \
        (t)(uintptr_t)v;                                                \
    })
#define CHI_ALIGN_CAST(x, t) _CHI_ALIGN_CAST(CHI_GENSYM, (x), t)

#if CHI_COUNT_ENABLED
#  define chiCount(c, n)       (c += (n))
#  define chiCountObject(c, n) ({ ++(c).count; (c).words += (n); })
#  define chiCountAdd(a, b)    ({ (a).count += (b).count; (a).words += (b).words; })
#else
#  define chiCount(c, n)       ({})
#  define chiCountObject(c, n) ({})
#  define chiCountAdd(a, b)    ({})
#endif
