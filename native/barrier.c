#include "runtime.h"
#include "barrier.h"
#include "thread.h"
#include "bit.h"
#include <chili/object/thunk.h>

CHI_INL bool casDirtyBit(Chili c, uintptr_t mask) {
    Chili* payload = chiPayload(c);
    ChiBlock* block = chiBlock(payload, mask);
    size_t idx = chiBlockIndex(block, payload);
    return chiAtomicBitCas((_Atomic(uint8_t)*)block->marked, idx, false, true, memory_order_relaxed);
}

/**
 * Dijkstra barrier (mark target value gray if source is black)
 * Also called forward barrier since it advances the gc.
 *
 * Source is black:
 * 1. source (black) -> old target value (not white)
 * 2. source (black) -> new target value (gray)
 *
 * Source is not black:
 * 1. source (not black) -> old target value (?)
 * 2. source (not black) -> new target value (?)
 */
static void writeFieldBarrier(ChiProcessor* proc, Chili source, Chili val) {
    CHI_ASSERT(chiMutable(source));
    CHI_ASSERT(!chiRaw(chiType(source)));

    if (!chiReference(val))
        return;

    const ChiRuntimeOption* opt = &proc->rt->option;
    if (CHI_UNLIKELY(opt->gc.mode == CHI_GC_NONE))
        return;

    uintptr_t mask = CHI_MASK(opt->block.size);
    ChiGen sourceGen = chiGen(source, mask), valGen = chiGen(val, mask);

    if (sourceGen == CHI_GEN_MAJOR) {
        ChiObject* sourceObj = chiObject(source);
        if (CHI_GEN_MAJOR > valGen && chiObjectCasDirty(sourceObj, false, true))
            chiBlockVecPush(proc->gcPS.dirty, source);
        if (proc->rt->gc.msPhase == CHI_MS_PHASE_MARK
            && valGen == CHI_GEN_MAJOR
            && chiObjectColor(sourceObj) == CHI_BLACK) {
            ChiObject* valObj = chiObject(val);
            if (chiObjectCasColor(valObj, CHI_WHITE, CHI_GRAY))
                chiBlockVecPush(&proc->gcPS.grayList, val);
        }
    } else if (sourceGen > valGen && casDirtyBit(source, mask)) {
        chiBlockVecPush(proc->gcPS.dirty + sourceGen, source);
    }
}

void chiWriteField(ChiProcessor* proc, Chili source, ChiAtomic* field, Chili val) {
    chiAtomicStore(field, val);
    writeFieldBarrier(proc, source, val);
}

bool chiCasField(ChiProcessor* proc, Chili source, ChiAtomic* field, Chili expected, Chili desired) {
    if (chiAtomicCas(field, expected, desired)) {
        writeFieldBarrier(proc, source, desired);
        return true;
    }
    return false;
}

/**
 * Steele barrier (mark source gray for rescan if source is black)
 * Also called backward barrier since it reverts some gc progress.
 *
 * Source is black:
 * 1. source (black)
 * 2. source (gray)
 *
 * source is not black: do nothing
 */
void chiWriteBarrier(ChiProcessor* proc, Chili source) {
    CHI_ASSERT(chiMutable(source));
    CHI_ASSERT(!chiRaw(chiType(source)));

    const ChiRuntimeOption* opt = &proc->rt->option;
    if (CHI_UNLIKELY(opt->gc.mode == CHI_GC_NONE))
        return;

    uintptr_t mask = CHI_MASK(opt->block.size);
    ChiGen gen = chiGen(source, mask);

    if (gen == CHI_GEN_MAJOR) {
        ChiObject* sourceObj = chiObject(source);
        if (chiObjectCasDirty(sourceObj, false, true))
            chiBlockVecPush(proc->gcPS.dirty, source);
        if (proc->rt->gc.msPhase == CHI_MS_PHASE_MARK
            && chiObjectCasColor(sourceObj, CHI_BLACK, CHI_GRAY))
            chiBlockVecPush(&proc->gcPS.grayList, source);
    } else if (gen && casDirtyBit(source, mask)) {
        chiBlockVecPush(proc->gcPS.dirty + gen, source);
    }
}

bool chiThunkCas(ChiProcessor* proc, Chili thunk, Chili expected, Chili desired) {
    CHI_ASSERT(chiType(expected) == CHI_THUNKFN);
    CHI_ASSERT(chiType(desired) == CHI_THUNKFN);
    return chiCasField(proc, thunk, &chiToThunk(thunk)->val, expected, desired);
}
