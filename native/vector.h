#include "mem.h"

#define VEC_APPEND CHI_CAT(VEC_PREFIX, Append)
#define VEC_REMOVE CHI_CAT(VEC_PREFIX, Remove)
#define VEC_PUSH   CHI_CAT(VEC_PREFIX, Push)
#define VEC_FREE   CHI_CAT(VEC_PREFIX, Free)
#define VEC_GROW   CHI_CAT(VEC_PREFIX, Grow)
#define VEC_ELEM   CHI_CAT(VEC, Elem)

typedef VEC_TYPE VEC_ELEM;

#ifndef NOVEC
typedef struct {
    size_t used, capacity;
    VEC_ELEM*  elem;
} VEC;
#endif

static void VEC_GROW(VEC* vec, size_t nelems) {
    if (vec->capacity < vec->used + nelems) {
        do {
            vec->capacity = vec->capacity ? (vec->capacity * CHI_VEC_GROWTH) / 100 : 1;
        } while (vec->capacity < vec->used + nelems);
        vec->elem = (VEC_ELEM*)chiRealloc(vec->elem, vec->capacity * sizeof (VEC_ELEM));
    }
}

CHI_INL void VEC_APPEND(VEC* vec, const VEC_ELEM* elems, size_t nelems) {
    CHI_ASSERT(vec->elem || !vec->capacity);
    CHI_ASSERT(vec->used <= vec->capacity);
    if (CHI_UNLIKELY(vec->capacity < vec->used + nelems))
        VEC_GROW(vec, nelems);
    memcpy(vec->elem + vec->used, elems, nelems * sizeof (VEC_ELEM));
    vec->used += nelems;
}

CHI_INL void VEC_REMOVE(VEC* vec, size_t i) {
    CHI_ASSERT(i < vec->used);
    --vec->used;
    memmove(vec->elem + i, vec->elem + i + 1, sizeof (VEC_ELEM) * (vec->used - i));
}

CHI_INL void VEC_PUSH(VEC* vec, VEC_ELEM elem) {
    VEC_APPEND(vec, &elem, 1);
}

static void VEC_FREE(VEC* vec) {
    if (vec->elem) {
        chiFree(vec->elem);
        vec->capacity = vec->used = 0;
        vec->elem = 0;
    }
}

#undef VEC_APPEND
#undef VEC_REMOVE
#undef VEC_PUSH
#undef VEC_FREE
#undef VEC_GROW
#undef VEC_ELEM
#undef VEC_PREFIX
#undef VEC_TYPE
#undef VEC
#undef NOVEC

#ifndef CHI_VEC_AT
#  define CHI_VEC_AT(v, i) ((v)->elem[({ size_t _i = (i); CHI_ASSERT((v)->elem && _i < (v)->used); _i; })])
#endif
