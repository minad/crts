#pragma once

#include "promote.h"
#include "runtime.h"

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

CHI_INL void _chiBarrierDirty(ChiProcessor* proc, Chili dst, Chili val) {
    ChiObject* obj = chiObject(dst);
    if (chiGen(val) != CHI_GEN_MAJOR && !chiObjectDirty(obj)) {
        chiObjectSetDirty(obj, true);
        chiBlockVecPush(&proc->heap.majorDirty, dst);
    }
}

CHI_INL void _chiBarrierMark(ChiProcessor* proc, Chili old, Chili val) {
    ChiLocalGC* gc = &proc->gc;
    if (gc->barrier) {
        chiGrayMarkUnboxed(gc, old);
        if (gc->barrier == CHI_BARRIER_SYNC)
            chiGrayMarkUnboxed(gc, val);
    }
}

CHI_INL void chiFieldWriteMajor(ChiProcessor* proc, Chili dst, ChiField* field, Chili val) {
    CHI_ASSERT(_chiBarrierMutable(chiType(dst)));
    CHI_ASSERT(chiUnboxed(val) || chiType(val) != CHI_STACK);
    CHI_ASSERT(chiGen(dst) == CHI_GEN_MAJOR);

    if (!chiUnboxed(val)) {
        if (chiObjectShared(chiObject(dst))) {
            CHI_ASSERT(!chiObjectDirty(chiObject(dst)));
            chiPromoteShared(proc, &val);
        } else {
            _chiBarrierDirty(proc, dst, val);
        }
    }

    _chiBarrierMark(proc, chiFieldRead(field), val);
    chiAtomicWrite(field, val);
}

CHI_INL void chiFieldWrite(ChiProcessor* proc, Chili dst, ChiField* field, Chili val) {
    CHI_ASSERT(_chiBarrierMutable(chiType(dst)));
    CHI_ASSERT(chiUnboxed(val) || chiType(val) != CHI_STACK);

    if (chiGen(dst) == CHI_GEN_MAJOR)
        chiFieldWriteMajor(proc, dst, field, val);
    else
        chiFieldInit(field, val);
}

CHI_INL bool chiFieldCasMajor(ChiProcessor* proc, Chili dst, ChiField* field, Chili old, Chili val) {
    CHI_ASSERT(_chiBarrierMutable(chiType(dst)));
    CHI_ASSERT(chiGen(dst) == CHI_GEN_MAJOR);

    if (chiObjectShared(chiObject(dst))) {
        CHI_ASSERT(!chiObjectDirty(chiObject(dst)));
        if (!chiUnboxed(val))
            chiPromoteShared(proc, &val);

        _chiBarrierMark(proc, old, val);
        return chiAtomicCas(field, old, val);
    }

    if (chiIdentical(chiFieldRead(field), old)) {
        if (!chiUnboxed(val))
            _chiBarrierDirty(proc, dst, val);
        _chiBarrierMark(proc, old, val);
        chiAtomicWrite(field, val);
        return true;
    }

    return false;
}
