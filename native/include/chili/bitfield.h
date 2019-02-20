/**
 * @file
 * Macros used to maintain bitfields, which need a precise layout.
 * The bitfields provide a certain level of typesafety.
 * See object.h and chili/runtime.h for examples of use.
 */
#define _CHI_BF_SHIFT(s, l, t) (s)
#define _CHI_BF_LEN(s, l, t)   (l)
#define _CHI_BF_TYPE(s, l, t)  t
#define CHI_BF_SHIFT(n)        _CHI_BF_SHIFT n
#define CHI_BF_LEN(n)          _CHI_BF_LEN n
#define CHI_BF_TYPE(n)         _CHI_BF_TYPE n
#define CHI_BF_OVF(n)          (1ULL << CHI_BF_LEN(n))
#define CHI_BF_MASK_0(n)       (CHI_BF_OVF(n) - 1)
#define CHI_BF_MASK_S(n)       (CHI_BF_MASK_0(n) << CHI_BF_SHIFT(n))
#define CHI_BF_INIT(n, x)                                               \
    ({                                                                  \
        CHI_BF_TYPE(n) _x = (x);                                        \
        ChiWord _w = (ChiWord)_x;                                       \
        CHI_ASSERT(_w < CHI_BF_OVF(n)); /*CHECK OVERFLOW*/              \
        (_w << CHI_BF_SHIFT(n));                                        \
    })
#define CHI_BF_GET(n, x)     ((CHI_BF_TYPE(n))(((x) & CHI_BF_MASK_S(n)) >> CHI_BF_SHIFT(n)))
#define CHI_BF_SET(n, x, y)  ({ x = ((x) & ~CHI_BF_MASK_S(n)) | CHI_BF_INIT(n, (y)); ({}); })

#define CHI_BF_ATOMIC_CAS(n, x, oldv, newv)                             \
    ({                                                                  \
        CHI_BF_TYPE(n) _oldv = (oldv), _newv = (newv);                  \
        bool _ret;                                                      \
        for (;;) {                                                      \
            ChiWord                                                     \
                _old = atomic_load_explicit(&(x), memory_order_relaxed), \
                _new = _old;                                            \
            if (CHI_BF_GET(n, _old) != _oldv) {                         \
                _ret = false;                                           \
                break;                                                  \
            }                                                           \
            CHI_BF_SET(n, _new, _newv);                                 \
            if (atomic_compare_exchange_weak(&(x), &_old, _new)) {      \
                _ret = true;                                            \
                break;                                                  \
            }                                                           \
        }                                                               \
        _ret;                                                           \
    })

#define CHI_BF_ATOMIC_SET(n, x, newv)                                   \
    ({                                                                  \
        CHI_BF_TYPE(n) _newv = (newv);                                  \
        for (;;) {                                                      \
            ChiWord                                                     \
                _old = atomic_load_explicit(&(x), memory_order_relaxed), \
                _new = _old;                                            \
            CHI_BF_SET(n, _new, _newv);                                 \
            if (atomic_compare_exchange_weak(&(x), &_old, _new))        \
                break;                                                  \
        }                                                               \
    })
