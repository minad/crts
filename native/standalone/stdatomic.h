#pragma once

#include <stddef.h>

typedef enum {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

// TODO: Test standalone libc on multithreaded platforms
#if 1

/*
 * "Atomics" for single threaded applications
 */

#define _atomic_nop(x) (false ? (void)(x) : ({}))

#define atomic_exchange_explicit(p, v, m)                       \
    ({                                                          \
        _atomic_nop(m);                                         \
        typeof(p) _p = (p);                                     \
        typeof(*_p) _v = (v), _o = *_p;                         \
        *_p = _v, _o;                                           \
    })
#define atomic_store_explicit(p, v, m) ({ _atomic_nop(m); *(p) = (v); })
#define atomic_load_explicit(p, m)     ({ _atomic_nop(m); *(p); })
#define atomic_compare_exchange_weak_explicit(p, v, d, s, f) \
    atomic_compare_exchange_strong_explicit((p), (v), (d), (s), (f))
#define atomic_compare_exchange_strong_explicit(p, v, d, s, f)  \
    ({                                                          \
        _atomic_nop(s); _atomic_nop(f);                         \
        typeof(p) _p = (p), _v = (v);                           \
        typeof(*_p) _d = (d);                                   \
        *_p == *_v && (*_p = _d, 1);                            \
    })
#define atomic_fetch_add_explicit(p, v, m)      \
    ({                                          \
        _atomic_nop(m);                         \
        typeof(p) _p = (p);                     \
        typeof(*_p) _v = (v), _o = *_p;         \
        *_p += _v, _o;                          \
    })
#define atomic_fetch_sub_explicit(p, v, m)      \
    ({                                          \
        _atomic_nop(m);                         \
        typeof(p) _p = (p);                     \
        typeof(*_p) _v = (v), _o = *_p;         \
        *_p -= _v, _o;                          \
    })

#else

/*
 * Atomics for multi threaded applications
 */

#define atomic_store_explicit(p, v, m)                                  \
  ({                                                                    \
      __auto_type _p = (p);                                             \
      typeof (*_p) _v = (v);                                            \
      memory_order _m = (m);                                            \
      __atomic_store(_p, &_v, (int)_m);                                 \
  })
#define atomic_load_explicit(p, m)                                      \
    ({                                                                  \
        __auto_type _p = (p);                                           \
        typeof (*_p) _v;                                                \
        memory_order _m = (m);                                          \
        __atomic_load(_p, &_v, (int)_m);                                \
        _v;                                                             \
  })
#define atomic_compare_exchange_weak_explicit(p, v, d, s, f)            \
    ({                                                                  \
        typeof(p) _p = (p), _v = (v);                                   \
        typeof (*_p) _d = (d);                                          \
        memory_order _s = (s), _f = (f);                                \
        __atomic_compare_exchange(_p, _v, &_d, 1, (int)_s, (int)_f);    \
    })
#define atomic_compare_exchange_strong_explicit(p, v, d, s, f)          \
    ({                                                                  \
        typeof(p) _p = (p), _v = (v);                                   \
        typeof (*_p) _d = (d);                                          \
        memory_order _s = (s), _f = (f);                                \
        __atomic_compare_exchange(_p, _v, &_d, 0, (int)_s, (int)_f);    \
    })
#define atomic_fetch_add_explicit(p, v, m)                              \
  ({                                                                    \
      __auto_type _p = (p);                                             \
      typeof (*_p) _v = (v);                                            \
      memory_order _m = (m);                                            \
      __atomic_fetch_add(_p, _v, (int)_m);                              \
  })
#define atomic_fetch_sub_explicit(p, v, m)                              \
  ({                                                                    \
      __auto_type _p = (p);                                             \
      typeof (*_p) _v = (v);                                            \
      memory_order _m = (m);                                            \
      __atomic_fetch_sub(_p, _v, (int)_m);                              \
  })

#endif
