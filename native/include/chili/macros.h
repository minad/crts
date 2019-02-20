/*
 * All inlined functions which belong to the public API must be marked with CHI_API_INL
 * such that they are available at runtime for FFI in the interpreter.
 *
 * Furthermore all extern functions (even private ones) must be marked with CHI_API
 * such that they are available for dynamic linking with shared objects in the interpreter.
 */
#ifndef CHI_API_INL
#  define CHI_API_INL   CHI_INL
#endif

#ifdef _WIN32
#  define CHI_API __attribute__ ((dllexport, visibility("default")))
#else
#  define CHI_API __attribute__ ((visibility("default")))
#endif

/**
 * Is a macro defined (empty) or 1
 */
#define CHI_DEFINED(x)           _CHI_DEFINED1(x)
#define _CHI_DEFINED1(x)         _CHI_DEFINED2(_CHI_DEFINED_TEST##x)
#define _CHI_DEFINED2(x)         _CHI_DEFINED3(x 1, 0)
#define _CHI_DEFINED3(x, y, ...) y
#define _CHI_DEFINED_TEST1       ,
#define _CHI_DEFINED_TEST        ,

/**
 * Internal functions which are exposed in the headers
 * must be marked with CHI_PRIVATE. The private.h header
 * makes them available to the runtime system.
 */
#define CHI_PRIVATE(n)        _##n

/**
 * In debugging mode, we use a newtype-style wrappers to
 * keep casts explicit. Since the wrappers seems
 * to prevent vectorization in some cases,
 * we use the plain type in release mode.
 *
 * On 32 bit we also avoid the wrapper since it
 * seems to mess with the calling conventions for struct passing.
 */
#if defined(NDEBUG) || CHI_ARCH_BITS == 32
#  define CHI_NEWTYPE(name, type) typedef type Chi##name;
#  define _CHI_UN(name, value)    value
#  define CHI_WRAP(name, value)   value
#else
#  define CHI_NEWTYPE(name, type) typedef struct { type _wrapped_Chi##name; } Chi##name;
#  define _CHI_UN(name, value)    value._wrapped_Chi##name
#  define CHI_WRAP(name, value)   (Chi##name){value}
#endif
#define CHI_UN(name, value)      (_CHI_UN(name, (value)))
#define CHI_UNP(name, ptr)       (&CHI_UN(name, *(ptr)))

#define CHI_AUTO(n, i, f)     typeof (i) n __attribute__ ((cleanup(chi_auto_##f))) = (i)
#define CHI_DEFINE_AUTO(t, f) CHI_INL void chi_auto_##f(t* p) { if (*p) f(*p); }
#define CHI_KiB(s)            ((s) * 1024ULL)
#define CHI_MiB(s)            (1024ULL * CHI_KiB(s))
#define CHI_GiB(s)            (1024ULL * CHI_MiB(s))
#define CHI_TiB(s)            (1024ULL * CHI_GiB(s))
#define _CHI_WARN_PRAGMA(x)   _Pragma(CHI_STRINGIZE(GCC diagnostic x))
#define _CHI_WARN_OFF1(x)     _CHI_WARN_PRAGMA(ignored CHI_STRINGIZE(CHI_CAT(-W, x)))
#define _CHI_WARN_OFF2(x,y)   _CHI_WARN_OFF1(x) _CHI_WARN_OFF1(y)
#define _CHI_WARN_OFF3(x,y,z) _CHI_WARN_OFF2(x, y) _CHI_WARN_OFF1(z)
#define CHI_WARN_OFF(...)     _CHI_WARN_PRAGMA(push) CHI_OVERLOAD(_CHI_WARN_OFF, ##__VA_ARGS__)(__VA_ARGS__)
#define CHI_WARN_ON           _CHI_WARN_PRAGMA(pop)
#define CHI_NOINL             __attribute__ ((noinline))
#define _CHI_CAT(x, y)        x ## y
#define CHI_CAT(x, y)         _CHI_CAT(x, y)
#define _CHI_STRINGIZE(x)     #x
#define CHI_STRINGIZE(x)      _CHI_STRINGIZE(x)
#define CHI_INCLUDE(x)        CHI_STRINGIZE(x.h)
#define CHI_MUST_BE_VAR(x)    ({ struct { int x; } CHI_UNUSED(must_be_var); })
#define CHI_MUST_BE_STRING(s) ""s""
#define CHI_STRSIZE(s)        (sizeof (CHI_MUST_BE_STRING(s)) - 1)
#define _CHI_IS_ARRAY(x)      (!__builtin_types_compatible_p(typeof(x), typeof(&((x)[0]))))
#define CHI_STATIC_FAIL(x)    sizeof (int[-(!(x))])
#define CHI_DIM(x)            __builtin_choose_expr(_CHI_IS_ARRAY(x), sizeof (x) / sizeof ((x)[0]), CHI_STATIC_FAIL(_CHI_IS_ARRAY(x)))
#define CHI_PACKED            __attribute__ ((packed))
#define CHI_ALIGNED(n)        __attribute__ ((aligned(n)))
#define CHI_WORD_ALIGNED      CHI_ALIGNED(CHI_WORDSIZE)
#define _CHI_STRUCTOR(p, s, c)   static __attribute__((s(p))) void c(void)
#define CHI_EARLY_CONSTRUCTOR _CHI_STRUCTOR(101, constructor, CHI_GENSYM)
#define CHI_CONSTRUCTOR       _CHI_STRUCTOR(102, constructor, CHI_GENSYM)
#define CHI_DESTRUCTOR        _CHI_STRUCTOR(102, destructor, CHI_GENSYM)
#define CHI_WEAK              __attribute__ ((weak))
#define CHI_ALIAS(n)          __attribute__ ((weak, alias(CHI_STRINGIZE(n))))
#define CHI_MALLOC            __attribute__ ((malloc))
#define CHI_UNUSED(x)         CHI_CAT(_chi_unused, x) __attribute__ ((unused))
#define CHI_NOWARN_UNUSED(x)  ({ CHI_MUST_BE_VAR(x); (void)x; })
#define _CHI_IGNORE_RESULT(r, x) ({ typeof(x) r = x; CHI_NOWARN_UNUSED(r); })
#define CHI_IGNORE_RESULT(x)     _CHI_IGNORE_RESULT(CHI_GENSYM, x)
#define CHI_WU                __attribute__ ((warn_unused_result))
#define CHI_RET_NONNULL       __attribute__ ((returns_nonnull))
#define CHI_ASSUMEALIGNED(n)  __attribute__ ((assume_aligned(n)))
#define CHI_COLD              __attribute__ ((cold))
#define CHI_GENSYM            CHI_CAT(_chi_gensym_,__COUNTER__)
#define _CHI_MM(v,w,x,y,o)    ({ typeof(x) v = (x), w = (y); v o w ? v : w; })
#define CHI_MIN(x, y)         _CHI_MM(CHI_GENSYM, CHI_GENSYM, x, y, <)
#define CHI_MAX(x, y)         _CHI_MM(CHI_GENSYM, CHI_GENSYM, x, y, >)
#define CHI_CLAMP(x, a, b)    CHI_MAX(CHI_MIN(x, b), a)
#define _CHI_CMP(v,w,x,y)     ({ typeof(x) v = (x), w = (y); (v > w) - (v < w); })
#define CHI_CMP(x, y)         _CHI_CMP(CHI_GENSYM, CHI_GENSYM, x, y)
#define CHI_NOP(x)            (false ? (void)(x) : ({}))
#define CHI_DIV_ROUNDUP(i, r) (((i) + ((r) - 1)) / (r))
#define CHI_ROUNDUP(i, r)     (CHI_DIV_ROUNDUP((i), (r)) * (r))
#define CHI_BYTES_TO_WORDS(i) CHI_DIV_ROUNDUP((i), CHI_WORDSIZE)
#define CHI_SIZEOF_WORDS(x)   CHI_BYTES_TO_WORDS(sizeof (x))
#define CHI_FIELD_SIZE(s, f)  (sizeof (((s*)0)->f))
#define CHI_FIELD_TYPE(s, f)  typeof (((s*)0)->f)
#define _CHI_SWAP(v, x, y)    ({ typeof(x) v = (x); (x) = (y); (y) = v; ({}); })
#define CHI_SWAP(x, y)        _CHI_SWAP(CHI_GENSYM, x, y)
#define CHI_MASK(n)           (~((n) - 1))
#define CHI_CLEAR(p)          __builtin_choose_expr(!_CHI_IS_ARRAY(p), memset((p), 0, sizeof (*(p))), CHI_STATIC_FAIL(!_CHI_IS_ARRAY(p)))
#define CHI_CLEAR_ARRAY(p)    __builtin_choose_expr(_CHI_IS_ARRAY(p), memset((p), 0, sizeof (p)), CHI_STATIC_FAIL(_CHI_IS_ARRAY(p)))

#define _CHI_OVERLOAD(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,f, ...) f
#define CHI_OVERLOAD(f, ...)  _CHI_OVERLOAD(_0, ##__VA_ARGS__,f##9,f##8,f##7,f##6,f##5,f##4,f##3,f##2,f##1,f##0)
#define _CHI_IF0(...)
#define _CHI_IF1(...)         __VA_ARGS__
#define CHI_IF(a, ...)        CHI_CAT(_CHI_IF, a)(__VA_ARGS__)
#define _CHI_IFELSE0(b, ...)  __VA_ARGS__
#define _CHI_IFELSE1(b, ...)  b
#define CHI_IFELSE(a, b, ...) CHI_CAT(_CHI_IFELSE, a)(b, __VA_ARGS__)
#define CHI_AND(a, b)         CHI_IFELSE(a, (b), 0)

// Since we are using a custom printf implementation,
// we need a separate typechecker plugin.
#if defined(__clang__) && !defined(NDEBUG)
#  define CHI_FMT(n, m)         __attribute__ ((annotate("__fmt__ "#n" "#m)))
#else
#  define CHI_FMT(n, m)
#endif

#define _CHI_OUTER(fp, type, field, ptr)                                \
    ({                                                                  \
        const CHI_FIELD_TYPE(type, field)* fp = (ptr);                  \
        CHI_CAST(((const char*)fp - offsetof(type, field)), type*);     \
    })
#define CHI_OUTER(type, field, ptr) _CHI_OUTER(CHI_GENSYM, type, field, ptr)

#ifdef NDEBUG
#  ifdef __clang__
#    define CHI_ASSUME(cond) __builtin_assume(cond)
#  else
#    define CHI_ASSUME(cond) ((cond) ? ({}) : CHI_UNREACHABLE)
#endif
#else
#  define CHI_ASSUME(cond) CHI_ASSERT(cond)
#endif

// Bound value for the optimizer => loop unrolling
#define CHI_BOUNDED(n, max)                                             \
    ({                                                                  \
        CHI_MUST_BE_VAR(n);                                             \
        _Static_assert(__builtin_constant_p(max), "max not constant");   \
        CHI_ASSUME((n) <= (max));                                       \
    })
#define CHI_BOUNDED_DOWN(n, min)                                        \
    ({                                                                  \
        CHI_MUST_BE_VAR(n);                                             \
        _Static_assert(__builtin_constant_p(min), "min not constant");   \
        CHI_ASSUME((n) >= (min));                                       \
    })

#define _CHI_DIV(v, w, m, q, a, b)                    \
    ({                                                          \
        typeof(a) v = (a), w = (b), m = v % w, q = v / w;       \
        (typeof(v))(!m || (m < 0) == (w < 0) ? q : q - 1);      \
    })
#define CHI_DIV(a, b) _CHI_DIV(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, a, b)

// Weird. INT_MIN % -1 == 0 is well defined. But the processor still traps on it.
// We handle the special case here since for the primitive on other backends the value is well defined.
#define _CHI_MOD(v, w, m, r, a, b)                            \
    ({                                                                  \
        typeof(a) v = (a), w = (b), m, r = 0;                           \
        if (CHI_LIKELY(v != (((typeof(v))1) << (sizeof(v) * 8 - 1)) || w != -1)) { \
            m = v % w;                                                  \
            r = !m || (m < 0) == (w < 0) ? m : m + w;                   \
        }                                                               \
        r;                                                              \
    })
#define CHI_MOD(a, b) _CHI_MOD(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, a, b)

#define _CHI_REM(v, w, a, b)                                  \
    ({                                                                  \
        typeof(a) v = (a), w = (b);                                     \
        CHI_LIKELY(v != (((typeof(v))1) << (sizeof(v) * 8 - 1)) || w != -1) ? \
            v % w : 0;                                                  \
    })
#define CHI_REM(a, b) _CHI_REM(CHI_GENSYM, CHI_GENSYM, a, b)

#define _CHI_CAST(to, from, x, t)                                     \
    ({                                                                  \
        typeof(x) from = (x); t to;                                     \
        _Static_assert(sizeof (from) == sizeof (to), "Sizes in cast differ"); \
        memcpy(&to, &from, sizeof (t));                                 \
        to;                                                             \
    })
#define CHI_CAST(x, t) _CHI_CAST(CHI_GENSYM, CHI_GENSYM, (x), t)

#define CHI_CONST_CAST(x, t)                                            \
    ({                                                                  \
        _Static_assert(__builtin_types_compatible_p(const t, typeof(x)), "invalid const cast"); \
        (t)(uintptr_t)(x);                                              \
    })

#define _CHI_ALIGN_CAST(v, x, t)                                        \
    ({                                                                  \
        typeof(x) v = (x);                                              \
        CHI_ASSERT(((uintptr_t)v) % _Alignof(*v) == 0);                 \
        (t)(uintptr_t)v;                                                \
    })
#define CHI_ALIGN_CAST(x, t) _CHI_ALIGN_CAST(CHI_GENSYM, (x), t)

#if CHI_ARCH_BITS == 32
#  define CHI_ALIGN_CAST32(x, t) CHI_ALIGN_CAST(x, t)
#else
#  define CHI_ALIGN_CAST32(x, t) ((t)(x))
#endif
