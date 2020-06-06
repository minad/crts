#pragma once

#include "promote.h"
#include "runtime.h"

/**
 * Barrier specialization.
 *
 * - If the object always lives in the major heap,
 *   the code for minor writes can be omitted.
 *
 * - If the object always lives in the minor/major-local heap
 *   (and is never shared), the expensive shared check and
 *   the expensive promotion code can be omitted.
 *
 * - The barrier can be configured to use a card table
 */
CHI_FLAGTYPE(ChiBarrierFlags,
             CHI_BARRIER_GENERIC      = 0,
             CHI_BARRIER_ALWAYS_MAJOR = 1,
             CHI_BARRIER_ALWAYS_LOCAL = 2,
             CHI_BARRIER_CARD         = 4)

CHI_INL CHI_WU bool _chiBarrierMutable(ChiType t) {
    return t >= CHI_FIRST_MUTABLE && t <= CHI_LAST_MUTABLE;
}

/**
 * Atomic store with release semantics.
 * No reads or writes in the current thread can be reordered before this operation.
 * All writes in this threads will be visible in other threads acquiring this variable.
 *
 * This primitive function should normally not be used directly,
 * since it does not provide a write barrier, which is necessary for the generational
 * garbage collector.
 */
CHI_INL void chiAtomicWrite(ChiField* c, Chili x) {
    atomic_store_explicit(CHI_UNP(Field, c), CHILI_UN(x), memory_order_release);
}

/**
 * Compare and swap operation. Implies full memory barrier (seq cst).
 *
 * This primitive function should normally not be used directly,
 * since it does not provide a write barrier, which is necessary for the generational
 * garbage collector.
 */
CHI_INL CHI_WU bool chiAtomicCas(ChiField* c, Chili expected, Chili desired) {
    return atomic_compare_exchange_strong_explicit(CHI_UNP(Field, c), &CHILI_UN(expected), CHILI_UN(desired),
                                                   memory_order_seq_cst, memory_order_relaxed);
}

CHI_INL void _chiBarrierDirty(ChiProcessor* proc, Chili dst, ChiField* field, Chili val, ChiBarrierFlags flags) {
    if (!chiRef(val) || chiGen(val) == CHI_GEN_MAJOR)
        return;

    ChiObject* obj = chiObject(dst);
    if (flags & CHI_BARRIER_CARD) {
        size_t idx = (size_t)(field - (ChiField*)obj->payload) / CHI_CARD_SIZE;
        bool* cardTable = (bool*)(obj->payload + obj->size);
        if (!cardTable[idx]) {
            cardTable[idx] = true;
            chiCardVecPush(&proc->heap.dirty.cards, (ChiCard){ dst, idx });
        }
    } else if (!obj->flags.dirty) {
        obj->flags.dirty = true;
        chiObjVecPush(&proc->heap.dirty.objects, dst);
    }
}

// Must be a macro, since old value must not be fetched if barrier is disabled
#define _CHI_BARRIER_MARK(proc, old, val)                       \
    ({                                                          \
        if (CHI_UNLIKELY((proc)->gc.markState.barrier))         \
            chiMarkBarrier(&(proc)->gc, (old), (val));      \
    })

CHI_INL void _chiFieldWriteMajor(ChiProcessor* proc, Chili dst, ChiField* field, Chili val, ChiBarrierFlags flags) {
    CHI_ASSERT(chiGen(dst) == CHI_GEN_MAJOR);

    ChiObject* dstObj = chiObject(dst);
    if (!(flags & CHI_BARRIER_ALWAYS_LOCAL) && CHI_UNLIKELY(chiObjectShared(dstObj))) {
        CHI_ASSERT(!dstObj->flags.dirty);
        if (chiRef(val))
            chiPromoteShared(proc, &val);
    } else {
        CHI_ASSERT(!chiObjectShared(dstObj));
        _chiBarrierDirty(proc, dst, field, val, flags);
    }

    _CHI_BARRIER_MARK(proc, chiFieldRead(field), val);
    chiAtomicWrite(field, val);
}

CHI_INL void chiFieldWrite(ChiProcessor* proc, Chili dst, ChiField* field, Chili val, ChiBarrierFlags flags) {
    CHI_ASSERT(_chiBarrierMutable(chiType(dst)));
    CHI_ASSERT(!chiRef(val) || chiType(val) != CHI_STACK);
    if ((flags & CHI_BARRIER_ALWAYS_MAJOR) || chiGen(dst) == CHI_GEN_MAJOR)
        _chiFieldWriteMajor(proc, dst, field, val, flags);
    else
        chiFieldInit(field, val);
}

CHI_INL bool _chiFieldCasMajor(ChiProcessor* proc, Chili dst, ChiField* field, Chili old, Chili val, ChiBarrierFlags flags) {
    ChiObject* dstObj = chiObject(dst);
    if (!(flags & CHI_BARRIER_ALWAYS_LOCAL) && CHI_UNLIKELY(chiObjectShared(dstObj))) {
        CHI_ASSERT(!dstObj->flags.dirty);
        if (chiRef(val))
            chiPromoteShared(proc, &val);

        _CHI_BARRIER_MARK(proc, old, val);
        return chiAtomicCas(field, old, val);
    }
    CHI_ASSERT(!chiObjectShared(dstObj));

    if (chiIdentical(chiFieldRead(field), old)) {
        _chiBarrierDirty(proc, dst, field, val, flags);
        _CHI_BARRIER_MARK(proc, old, val);
        chiAtomicWrite(field, val);
        return true;
    }

    return false;
}

CHI_INL bool chiFieldCas(ChiProcessor* proc, Chili dst, ChiField* field, Chili old, Chili val, ChiBarrierFlags flags) {
    CHI_ASSERT(_chiBarrierMutable(chiType(dst)));

    if ((flags & CHI_BARRIER_ALWAYS_MAJOR) || chiGen(dst) == CHI_GEN_MAJOR)
        return _chiFieldCasMajor(proc, dst, field, old, val, flags);

    if (chiIdentical(chiFieldRead(field), old)) {
        chiFieldInit(field, val);
        return true;
    }

    return false;
}
