/**
 * @file
 * Macros used to maintain bitfields, which need a precise layout.
 * The bitfields provide a certain level of typesafety.
 * See object.h and chili/runtime.h for examples of use.
 */
#define _CHI_BF_SHIFT(s, l, a, t) (s)
#define _CHI_BF_LEN(s, l, a, t)   (l)
#define _CHI_BF_ALIGN(s, l, a, t) (a)
#define _CHI_BF_TYPE(s, l, a, t)  t
#define CHI_BF_SHIFT(n)           _CHI_BF_SHIFT n
#define CHI_BF_LEN(n)             _CHI_BF_LEN n
#define CHI_BF_TYPE(n)            _CHI_BF_TYPE n
#define CHI_BF_ALIGN(n)           _CHI_BF_ALIGN n
#define CHI_BF_MASK(n)            ((UINT64_C(1) << CHI_BF_LEN(n)) - 1)
#define CHI_BF_SHIFTED_MASK(n)    (CHI_BF_MASK(n) << CHI_BF_SHIFT(n))
#define CHI_BF_MAX(n)             (CHI_BF_MASK(n) << CHI_BF_ALIGN(n))
#define CHI_BF_ALIGNED_SHIFT(n)   (CHI_BF_SHIFT(n) - CHI_BF_ALIGN(n))
#define CHI_BF_GET(n, x)          ((CHI_BF_TYPE(n))(((x) >> CHI_BF_ALIGNED_SHIFT(n)) & CHI_BF_MAX(n)))
#define CHI_BF_SET(n, x, y)       ({ x = ((x) & ~CHI_BF_SHIFTED_MASK(n)) | CHI_BF_INIT(n, (y)); ({}); })

#define CHI_BF_INIT(n, x)                                               \
    ({                                                                  \
        CHI_BF_TYPE(n) _x = (x);                                        \
        ChiWord _w = (ChiWord)_x;                                       \
        _Static_assert(CHI_BF_SHIFT(n) >= CHI_BF_ALIGN(n), "alignment"); \
        CHI_ASSERT(!(_w & ((UINT64_C(1) << CHI_BF_ALIGN(n)) - 1))); /*CHECK ALIGNMENT*/ \
        CHI_ASSERT(_w <= CHI_BF_MAX(n)); /*CHECK OVERFLOW*/             \
        _w << CHI_BF_ALIGNED_SHIFT(n);                                  \
    })
