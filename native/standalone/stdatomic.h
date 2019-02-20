#pragma once

#include <stddef.h>

typedef enum {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

#define atomic_init(p, v)                       atomic_store_explicit((p), (v), memory_order_relaxed)
#define atomic_store(p, v)                      atomic_store_explicit((p), (v), memory_order_seq_cst)
#define atomic_load(p)                          atomic_load_explicit((p), memory_order_seq_cst)
#define atomic_compare_exchange_weak(p, v, d)   atomic_compare_exchange_weak_explicit((p), (v), (d), memory_order_seq_cst, memory_order_seq_cst)
#define atomic_compare_exchange_strong(p, v, d) atomic_compare_exchange_strong_explicit((p), (v), (d), memory_order_seq_cst, memory_order_seq_cst)

#if 1

/*
 * "Atomics" for single threaded applications
 */

#define atomic_store_explicit(p, v, m) ({ CHI_NOP(m); *(p) = (v); })
#define atomic_load_explicit(p, m)     ({ CHI_NOP(m); *(p); })
#define atomic_compare_exchange_weak_explicit(p, v, d, s, f) \
    atomic_compare_exchange_strong_explicit((p), (v), (d), (s), (f))
#define atomic_compare_exchange_strong_explicit(p, v, d, s, f)  \
    ({                                                          \
        CHI_NOP(s); CHI_NOP(f);                                 \
        typeof(p) _p = (p), _v = (v);                           \
        typeof(*_p) _d = (d);                                   \
        *_p == *_v && (*_p = _d, 1);                            \
    })
#else

/*
 * Atomics for multi threaded applications
 */

#define atomic_store_explicit(p, v, m)                                  \
  ({                                                                    \
      __auto_type _p = (p);                                             \
      typeof (*_p) __atomic_store_tmp = (v);                            \
      memory_order _m = (m);                                            \
      __atomic_store(_p, &__atomic_store_tmp, (int)_m);                 \
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

#endif
