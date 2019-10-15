#include "../mem.h"

#define VEC_APPEND_MANY CHI_CAT(VEC_PREFIX, AppendMany)
#define VEC_APPEND      CHI_CAT(VEC_PREFIX, Append)
#define VEC_INSERT_MANY CHI_CAT(VEC_PREFIX, InsertMany)
#define VEC_INSERT      CHI_CAT(VEC_PREFIX, Insert)
#define VEC_DELETE      CHI_CAT(VEC_PREFIX, Delete)
#define VEC_FREE        CHI_CAT(VEC_PREFIX, Free)
#define VEC_GROW        CHI_CAT(VEC_PREFIX, Grow)
#define VEC_ELEM        CHI_CAT(VEC, Elem)

typedef VEC_TYPE VEC_ELEM;

#ifndef VEC_NOSTRUCT
typedef struct {
    size_t used, capacity;
    VEC_ELEM*  elem;
} VEC;
#endif

CHI_LOCAL void VEC_FREE(VEC* vec) {
    if (vec->elem) {
        chiFree(vec->elem);
        vec->capacity = vec->used = 0;
        vec->elem = 0;
    }
}

CHI_LOCAL void VEC_GROW(VEC* vec, size_t nelems) {
    if (vec->capacity < vec->used + nelems) {
        do {
            vec->capacity = vec->capacity ? (vec->capacity * CHI_VEC_GROWTH) / 100 : 1;
        } while (vec->capacity < vec->used + nelems);
        vec->elem = (VEC_ELEM*)chiRealloc(vec->elem, vec->capacity * sizeof (VEC_ELEM));
    }
}

CHI_INL void VEC_APPEND_MANY(VEC* vec, const VEC_ELEM* elems, size_t nelems) {
    CHI_ASSERT(vec->elem || !vec->capacity);
    CHI_ASSERT(vec->used <= vec->capacity);
    if (CHI_UNLIKELY(vec->capacity < vec->used + nelems))
        VEC_GROW(vec, nelems);
    memcpy(vec->elem + vec->used, elems, nelems * sizeof (VEC_ELEM));
    vec->used += nelems;
}

CHI_INL void VEC_INSERT_MANY(VEC* vec, size_t i, const VEC_ELEM* elems, size_t nelems) {
    CHI_ASSERT(i <= vec->used);
    CHI_ASSERT(vec->elem || !vec->capacity);
    CHI_ASSERT(vec->used <= vec->capacity);
    if (CHI_UNLIKELY(vec->capacity < vec->used + nelems))
        VEC_GROW(vec, nelems);
    if (i != vec->used)
        memmove(vec->elem + i + nelems, vec->elem + i, sizeof (VEC_ELEM) * (vec->used - i));
    memcpy(vec->elem + i, elems, nelems * sizeof (VEC_ELEM));
    vec->used += nelems;
}

CHI_INL void VEC_DELETE(VEC* vec, size_t i) {
    CHI_ASSERT(i < vec->used);
    --vec->used;
    memmove(vec->elem + i, vec->elem + i + 1, sizeof (VEC_ELEM) * (vec->used - i));
}

CHI_INL void VEC_APPEND(VEC* vec, VEC_ELEM elem) {
    VEC_APPEND_MANY(vec, &elem, 1);
}

CHI_INL void VEC_INSERT(VEC* vec, size_t i, VEC_ELEM elem) {
    VEC_INSERT_MANY(vec, i, &elem, 1);
}

#undef VEC
#undef VEC_NOSTRUCT
#undef VEC_APPEND
#undef VEC_APPEND_MANY
#undef VEC_INSERT
#undef VEC_INSERT_MANY
#undef VEC_DELETE
#undef VEC_ELEM
#undef VEC_FREE
#undef VEC_GROW
#undef VEC_PREFIX
#undef VEC_TYPE

#ifndef CHI_VEC_AT
#  define CHI_VEC_AT(v, i) ((v)->elem[({ size_t _i = (i); CHI_ASSERT((v)->elem && _i < (v)->used); _i; })])
#endif
