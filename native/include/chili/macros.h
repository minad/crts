/*
 * Inlined functions which should be available to the interpreter ffi/dynamic linking
 * must be marked with CHI_EXPORT_INL.
 */
#ifndef CHI_EXPORT_INL
#  define CHI_EXPORT_INL   CHI_INL
#endif

/*
 * Functions which should be available to the interpreter ffi/dynamic linking
 * must be marked with CHI_EXPORT. This applies also to CHI_PRIVATE functions.
 */
#ifdef _WIN32
#  define CHI_EXPORT __attribute__ ((dllexport, visibility("default")))
#else
#  define CHI_EXPORT __attribute__ ((visibility("default")))
#endif

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
#if defined(NDEBUG) || CHI_ARCH_32BIT
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

#if __has_attribute(flag_enum)
#  define CHI_FLAGTYPE(n, ...) typedef enum __attribute__((flag_enum)) { __VA_ARGS__ } n;
#else
#  define CHI_FLAGTYPE(n, ...) enum { __VA_ARGS__ }; typedef uint32_t n;
#endif

#define _CHI_WARN_PRAGMA(x)   _Pragma(CHI_STRINGIZE(GCC diagnostic x))
#define _CHI_WARN_OFF1(x)     _CHI_WARN_PRAGMA(ignored CHI_STRINGIZE(CHI_CAT(-W, x)))
#define _CHI_WARN_OFF2(x,y)   _CHI_WARN_OFF1(x) _CHI_WARN_OFF1(y)
#define _CHI_WARN_OFF3(x,y,z) _CHI_WARN_OFF2(x, y) _CHI_WARN_OFF1(z)
#define CHI_WARN_OFF(...)     _CHI_WARN_PRAGMA(push) CHI_OVERLOAD(_CHI_WARN_OFF, ##__VA_ARGS__)(__VA_ARGS__)
#define CHI_WARN_ON           _CHI_WARN_PRAGMA(pop)
#define _CHI_CAT(x, y)        x ## y
#define CHI_CAT(x, y)         _CHI_CAT(x, y)
#define CHI_CAT3(x, y, z)     CHI_CAT(x, CHI_CAT(y, z))
#define CHI_CAT4(x, y, z, w)  CHI_CAT(x, CHI_CAT(y, CHI_CAT(z, w)))
#define _CHI_STRINGIZE(x)     #x
#define CHI_STRINGIZE(x)      _CHI_STRINGIZE(x)
#define CHI_MUST_BE_VAR(x)    ({ struct { int x; } CHI_UNUSED(must_be_var); })
#define CHI_MUST_BE_STRING(s) ""s""
#define CHI_STRSIZE(s)        (sizeof (CHI_MUST_BE_STRING(s)) - 1)
#define _CHI_IS_ARRAY(x)      (!__builtin_types_compatible_p(typeof(x), typeof(&((x)[0]))))
#define CHI_STATIC_FAIL(x)    sizeof (int[-(!(x))])
#define CHI_SIZEOF_ARRAY(x)   __builtin_choose_expr(_CHI_IS_ARRAY(x), sizeof (x), CHI_STATIC_FAIL(_CHI_IS_ARRAY(x)))
#define CHI_SIZEOF_STRUCT(x)  __builtin_choose_expr(!_CHI_IS_ARRAY(x), sizeof (*(x)), CHI_STATIC_FAIL(!_CHI_IS_ARRAY(x)))
#define CHI_DIM(x)            (CHI_SIZEOF_ARRAY(x) / sizeof (*(x)))
#define CHI_NOINL             __attribute__ ((noinline))
#define CHI_PACKED            __attribute__ ((packed))
#define CHI_ALIGNED(n)        __attribute__ ((aligned(n)))
#define CHI_WORD_ALIGNED      CHI_ALIGNED(CHI_WORDSIZE)
#define CHI_WEAK              __attribute__ ((weak))
#define CHI_MALLOC            __attribute__ ((malloc))
#define CHI_WU                __attribute__ ((warn_unused_result))
#define CHI_RET_NONNULL       __attribute__ ((returns_nonnull))
#define CHI_ASSUME_ALIGNED(n) __attribute__ ((assume_aligned(n)))
#define CHI_ASSUME_WORD_ALIGNED CHI_ASSUME_ALIGNED(CHI_WORDSIZE)
#define CHI_LOCAL             static __attribute__ ((unused))
#define CHI_UNUSED(x)         CHI_CAT(_chi_unused, x) __attribute__ ((unused))
#define CHI_NOWARN_UNUSED(x)  ({ CHI_MUST_BE_VAR(x); (void)x; })
#define CHI_GENSYM            CHI_CAT(_chisym,__COUNTER__)
#define _CHI_CMP(v,w,x,y)     ({ typeof(x) v = (x), w = (y); (v > w) - (v < w); })
#define CHI_CMP(x, y)         _CHI_CMP(CHI_GENSYM, CHI_GENSYM, x, y)

#define _CHI_STRUCTOR(p, s, n) static __attribute__((s(p))) void n(void)
#define CHI_EARLY_CONSTRUCTOR(n)  _CHI_STRUCTOR(101, constructor, CHI_CAT(_chiConstructor_, n))
#define CHI_CONSTRUCTOR(n)        _CHI_STRUCTOR(102, constructor, CHI_CAT(_chiConstructor_, n))
#define CHI_DESTRUCTOR(n)         _CHI_STRUCTOR(102, destructor,  CHI_CAT(_chiDestructor_, n))

#define _CHI_OVERLOAD(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,f, ...) f
#define CHI_OVERLOAD(f, ...)  _CHI_OVERLOAD(_0, ##__VA_ARGS__,f##9,f##8,f##7,f##6,f##5,f##4,f##3,f##2,f##1,f##0)

#define _CHI_IF0(...)
#define _CHI_IF1(...)          __VA_ARGS__
#define _CHI_IF(a, ...)        CHI_CAT(_CHI_IF, a)(__VA_ARGS__)
#define CHI_IF(...)            _CHI_IF(__VA_ARGS__)

#define _CHI_UNLESS0(...)      __VA_ARGS__
#define _CHI_UNLESS1(...)
#define _CHI_UNLESS(a, ...)    CHI_CAT(_CHI_UNLESS, a)(__VA_ARGS__)
#define CHI_UNLESS(...)        _CHI_UNLESS(__VA_ARGS__)

#define _CHI_CHOICE0(b, ...)   __VA_ARGS__
#define _CHI_CHOICE1(b, ...)   b
#define _CHI_CHOICE(a, b, ...) CHI_CAT(_CHI_CHOICE, a)(b, __VA_ARGS__)
#define CHI_CHOICE(...)        _CHI_CHOICE(__VA_ARGS__)

#define CHI_AND(a, b)          CHI_CHOICE(a, (b), 0)

// Since we are using a custom printf implementation,
// we need a separate typechecker plugin.
#if defined(__clang__) && !defined(NDEBUG)
#  define CHI_FMT(n, m)         __attribute__ ((annotate("__fmt__ "#n" "#m)))
#else
#  define CHI_FMT(n, m)
#endif

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

#define _CHI_MM(v,w,x,y,o)    ({ typeof(x) v = (x), w = (y); v o w ? v : w; })
#define CHI_MIN(x, y)         _CHI_MM(CHI_GENSYM, CHI_GENSYM, x, y, <)
#define CHI_MAX(x, y)         _CHI_MM(CHI_GENSYM, CHI_GENSYM, x, y, >)
#define CHI_CLAMP(x, a, b)    CHI_MAX(CHI_MIN(x, b), a)

/**
 * Macro which can be used to undefine a value.
 * This is useful for the continuation registers.
 *
 * By marking a register as undefined, the current register
 * value must not be saved and can be overwritten.
 * This is useful for optimizations.
 *
 * In LLVM undefined values translate to the "undef"
 * primitive value.
 */
#define _CHI_UNDEF1(u, x) ({ typeof(x) u; memset(&u, CHI_POISON_UNDEF, sizeof (u)); x = u; })
#define _CHI_UNDEF0(u, x) ({ CHI_WARN_OFF(uninitialized) typeof(x) u; x = u; CHI_WARN_ON })
#define CHI_UNDEF(x)      CHI_CAT(_CHI_UNDEF, CHI_POISON_ENABLED)(CHI_GENSYM, x)

/**
 * Helper macro to define heap object types
 */
#define _CHI_OBJECT(NAME, Name, to, ...)                              \
    typedef struct CHI_PACKED CHI_WORD_ALIGNED { __VA_ARGS__; } Name; \
    CHI_INL CHI_WU Name* to(Chili c) {                                \
        CHI_ASSERT(_chiType(c) == NAME);                              \
        return (Name*)_chiRawPayload(c);                              \
    }
#define CHI_OBJECT(NAME, Name, ...)  _CHI_OBJECT(CHI##_##NAME, Chi##Name, _chiTo##Name, __VA_ARGS__)
