#define VEC_FREE  CHI_CAT(VEC_PREFIX, Free)
#define VEC_GROW  CHI_CAT(VEC_PREFIX, Grow)
#define VEC_HEAD  CHI_CAT(VEC_PREFIX, Head)
#define VEC_INIT  CHI_CAT(VEC_PREFIX, Init)
#define VEC_JOIN  CHI_CAT(VEC_PREFIX, Join)
#define VEC_NULL  CHI_CAT(VEC_PREFIX, Null)
#define VEC_POP   CHI_CAT(VEC_PREFIX, Pop)
#define VEC_PUSH  CHI_CAT(VEC_PREFIX, Push)

#include "../blockman.h"

#ifndef CHI_BLOCKVEC_FOREACH
#define _CHI_BLOCKVEC_FOREACH(_bp, p, e, v)                           \
    for (typeof(CHI_CAT(p, _begin)((v), 0)) _bp, e = CHI_CAT(p, _begin)((v), (ChiBlock**)&_bp); e; \
         e = CHI_CAT(p, _next)((v), e, (ChiBlock**)&_bp))
#define CHI_BLOCKVEC_FOREACH(p, e, v) _CHI_BLOCKVEC_FOREACH(CHI_GENSYM, p, e, (v))
#endif

#ifndef VEC_NOSTRUCT
typedef struct {
    ChiBlockList list;
    ChiBlockManager* manager;
} VEC;
#endif

CHI_INL void VEC_INIT(VEC* v, ChiBlockManager* manager) {
    chiBlockListInit(&v->list);
    v->manager = manager;
}

CHI_INL CHI_WU bool VEC_NULL(const VEC* v) {
    return chiBlockListNull(&v->list);
}

CHI_LOCAL CHI_WU ChiBlock* VEC_GROW(VEC* v) {
    ChiBlock* b = chiBlockManagerAlloc(v->manager);
    b->vec.ptr = (uint8_t*)CHI_ROUNDUP((uintptr_t)b->vec.base, sizeof (VEC_TYPE));
    b->vec.end = (uint8_t*)b + (v->manager->blockSize / sizeof (VEC_TYPE)) * sizeof (VEC_TYPE);
    CHI_ASSERT(CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*));
    chiBlockListPoison(b);
    chiBlockListPrepend(&v->list, b);
    return b;
}

CHI_INL void VEC_FREE(VEC* v) {
    chiBlockManagerFreeList(v->manager, &v->list);
}

CHI_INL CHI_WU ChiBlock* VEC_HEAD(const VEC* v) {
    return chiBlockListHead(&v->list);
}

CHI_INL CHI_WU bool VEC_POP(VEC* v, VEC_TYPE* x) {
    ChiBlock* b = VEC_HEAD(v);
    if (!b)
        return false;
    b->vec.ptr -= sizeof (VEC_TYPE);
    CHI_ASSERT(b->vec.ptr >= b->vec.base);
    *x = *CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*);
    if (CHI_UNLIKELY(b->vec.ptr < b->vec.base + sizeof (VEC_TYPE)))
        chiBlockManagerFree(v->manager, chiBlockListPop(&v->list));
    return true;
}

CHI_INL void VEC_PUSH(VEC* v, VEC_TYPE x) {
    ChiBlock* b = VEC_HEAD(v);
    if (CHI_UNLIKELY(!b || b->vec.ptr == b->vec.end))
        b = VEC_GROW(v);
    CHI_ASSERT(b->vec.ptr + sizeof (VEC_TYPE) <= b->vec.end);
    *CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*) = x;
    b->vec.ptr += sizeof (VEC_TYPE);
}

CHI_INL void VEC_JOIN(VEC* dst, VEC* src) {
    CHI_ASSERT(dst != src);
    CHI_ASSERT(dst->manager == src->manager);
    chiBlockListJoin(&dst->list, &src->list);
}

CHI_INL CHI_WU VEC_TYPE* CHI_CAT(VEC, _begin)(const VEC* v, ChiBlock** bp) {
    ChiBlock* b = VEC_HEAD(v);
    if (!b)
        return 0;
    CHI_ASSERT(b->vec.ptr >= b->vec.base + sizeof (VEC_TYPE));
    *bp = b;
    return CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*) - 1;
}

CHI_INL CHI_WU VEC_TYPE* CHI_CAT(VEC, _next)(const VEC* v, VEC_TYPE* p, ChiBlock** bp) {
    ChiBlock* b = *bp;
    if ((uint8_t*)p >= b->vec.base + sizeof (VEC_TYPE))
        return p - 1;
    b = chiBlockListNext(&v->list, b);
    if (!b)
        return 0;
    *bp = b;
    CHI_ASSERT(b->vec.ptr >= b->vec.base + sizeof (VEC_TYPE));
    return CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*) - 1;
}

#undef VEC_TYPE
#undef VEC_FREE
#undef VEC_GROW
#undef VEC_HEAD
#undef VEC_INIT
#undef VEC_JOIN
#undef VEC_NULL
#undef VEC_POP
#undef VEC_PREFIX
#undef VEC_PUSH
#undef VEC
