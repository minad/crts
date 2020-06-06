#define VEC_FREE     CHI_CAT(VEC_PREFIX, Free)
#define VEC_GROW     CHI_CAT(VEC_PREFIX, Grow)
#define VEC_HEAD     CHI_CAT(VEC_PREFIX, Head)
#define VEC_INIT     CHI_CAT(VEC_PREFIX, Init)
#define VEC_FROM     CHI_CAT(VEC_PREFIX, From)
#define VEC_JOIN     CHI_CAT(VEC_PREFIX, Join)
#define VEC_NULL     CHI_CAT(VEC_PREFIX, Null)
#define VEC_POP      CHI_CAT(VEC_PREFIX, Pop)
#define VEC_PUSH     CHI_CAT(VEC_PREFIX, Push)
#define VEC_TRANSFER CHI_CAT(VEC_PREFIX, Transfer)

#include "../block.h"

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

CHI_INL CHI_WU ChiBlock* VEC_HEAD(const VEC* v) {
    CHI_ASSERT(!chiBlockListNull(&v->list));
    return CHI_OUTER(ChiBlock, link, v->list.head.next);
}

CHI_INL CHI_WU bool VEC_NULL(const VEC* v) {
    ChiBlock* b = VEC_HEAD(v);
    bool null = b->vec.ptr < b->vec.base + sizeof (VEC_TYPE);
    CHI_ASSERT(!null || v->list.head.next == v->list.head.prev);
    return null;
}

CHI_LOCAL ChiBlock* VEC_GROW(VEC* v) {
    ChiBlock* b = chiBlockManagerAlloc(v->manager);
    b->vec.ptr = (uint8_t*)CHI_CEIL((uintptr_t)b->vec.base, sizeof (VEC_TYPE));
    b->vec.end = (uint8_t*)b + CHI_FLOOR(v->manager->blockSize, sizeof (VEC_TYPE));
    CHI_ASSERT(CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*));
    chiBlockListPoison(b);
    chiBlockListPrepend(&v->list, b);
    return b;
}

CHI_INL void VEC_INIT(VEC* v, ChiBlockManager* manager) {
    chiBlockListInit(&v->list);
    v->manager = manager;
    VEC_GROW(v);
    CHI_ASSERT(VEC_NULL(v));
}

CHI_INL void VEC_FROM(VEC* dst, VEC* src) {
    dst->manager = src->manager;
    chiBlockListInit(&dst->list);
    chiBlockListJoin(&dst->list, &src->list);
    CHI_POISON_STRUCT(src, CHI_POISON_DESTROYED);
}

CHI_INL void VEC_FREE(VEC* v) {
    chiBlockManagerFreeList(v->manager, &v->list);
    CHI_POISON_STRUCT(v, CHI_POISON_DESTROYED);
}

CHI_INL CHI_WU bool VEC_POP(VEC* v, VEC_TYPE* x) {
    ChiBlock* b = VEC_HEAD(v);
    if (CHI_UNLIKELY(b->vec.ptr < b->vec.base + 2 * sizeof (VEC_TYPE))) {
        if (VEC_NULL(v))
            return false;

        b->vec.ptr -= sizeof (VEC_TYPE);
        CHI_ASSERT(b->vec.ptr >= b->vec.base);
        *x = *CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*);

        if (v->list.head.next != v->list.head.prev) {
            chiBlockListDelete(&v->list, b);
            chiBlockManagerFree(v->manager, b);
        }

        return true;
    }

    b->vec.ptr -= sizeof (VEC_TYPE);
    CHI_ASSERT(b->vec.ptr >= b->vec.base);
    *x = *CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*);
    return true;
}

CHI_INL void VEC_PUSH(VEC* v, VEC_TYPE x) {
    ChiBlock* b = VEC_HEAD(v);
    if (CHI_UNLIKELY(b->vec.ptr == b->vec.end))
        b = VEC_GROW(v);
    CHI_ASSERT(b->vec.ptr + sizeof (VEC_TYPE) <= b->vec.end);
    *CHI_ALIGN_CAST(b->vec.ptr, VEC_TYPE*) = x;
    b->vec.ptr += sizeof (VEC_TYPE);
}

CHI_INL void VEC_JOIN(VEC* dst, VEC* src) {
    CHI_ASSERT(dst != src);
    CHI_ASSERT(dst->manager == src->manager);
    if (VEC_NULL(src))
        return;
    if (VEC_NULL(dst)) {
        ChiBlock* b = chiBlockListPop(&dst->list);
        CHI_ASSERT(chiBlockListNull(&dst->list));
        chiBlockListJoin(&dst->list, &src->list);
        chiBlockListAppend(&src->list, b);
    } else {
        chiBlockListJoin(&dst->list, &src->list);
        VEC_INIT(src, src->manager);
    }
}

CHI_INL void VEC_TRANSFER(VEC* dst, VEC* src) {
    CHI_ASSERT(dst != src);
    CHI_ASSERT(dst->manager == src->manager);
    CHI_ASSERT(!VEC_NULL(src));
    if (VEC_NULL(dst))
        chiBlockManagerFree(dst->manager, chiBlockListPop(&dst->list));
    chiBlockListAppend(&dst->list, chiBlockListPop(&src->list));
    if (chiBlockListNull(&src->list))
        VEC_INIT(src, src->manager);
}

CHI_INL CHI_WU VEC_TYPE* CHI_CAT(VEC, _begin)(const VEC* v, ChiBlock** bp) {
    if (VEC_NULL(v))
        return 0;
    ChiBlock* b = VEC_HEAD(v);
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

#undef VEC
#undef VEC_FREE
#undef VEC_GROW
#undef VEC_HEAD
#undef VEC_INIT
#undef VEC_FROM
#undef VEC_JOIN
#undef VEC_NULL
#undef VEC_POP
#undef VEC_PREFIX
#undef VEC_PUSH
#undef VEC_TRANSFER
#undef VEC_TYPE
