#include "runtime.h"
#include "error.h"
#include "event.h"

static void markDirty(ChiProcessor* proc, Chili c) {
    const ChiRuntimeOption* opt = &proc->rt->option;
    if (CHI_UNLIKELY(opt->gc.mode == CHI_GC_NONE))
        return;
    chiObjectSetDirty(chiObject(c), true);
    chiBlockVecPush(proc->gcPS.dirty, c);
}

Chili chiNewFlags(ChiType type, size_t size, uint32_t flags) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(size);
    Chili c;
    if (size <= CHI_MAX_UNPINNED) {
        if (!(flags & CHI_NEW_PIN))
            return chiBlockNew(&proc->nursery.outOfBandAlloc, type, size);
        c = chiHeapNewSmall(&proc->heapHandle, type, size, CHI_WHITE);
        CHI_ASSERT(chiSuccess(c));
    } else {
        ChiHeapFail fail = flags & CHI_NEW_TRY ? CHI_HEAP_MAYFAIL : CHI_HEAP_NOFAIL;
        c = chiHeapNewLarge(&proc->heapHandle, type, size, CHI_WHITE, fail);
        if (!chiSuccess(c)) {
            CHI_ASSERT(fail == CHI_HEAP_MAYFAIL);
            CHI_EVENT(proc, HEAP_ALLOC_FAILED,
                      .type=chiStringRef(chiTypeName(type)), .size=size);
            return CHI_FAIL;
        }
    }
    // Mark as dirty immediately, since object will be modified after allocation
    // In particular the object will be initialized with gen=0 references, violating the generational invariant!
    if (!chiRaw(type))
        markDirty(proc, c);
    return c;
}

/**
 * Allocate new object in an out-of-line block.
 * Usually bump allocation should be preferred.
 * If the object is large it will be directly allocated on the major heap.
 */
Chili chiNew(ChiType type, size_t size) {
    return chiNewFlags(type, size, CHI_NEW_DEFAULT);
}

/**
 * Allocate new object and ensure that it is pinned on the major heap.
 */
Chili chiNewPin(ChiType type, size_t size) {
    return chiNewFlags(type, size, CHI_NEW_PIN);
}

/**
 * Pin object by copying it to the major heap.
 *
 * For strings chiTryPinString should be used instead.
 * While chiTryPin will work for heap allocated strings,
 * it will fail for unboxed small strings.
 */
Chili chiTryPin(Chili c) {
    CHI_ASSERT(chiReference(c)); // Only reference types can be pinned
    if (chiPinned(c))
        return c;
    size_t s = chiSize(c);
    ChiType t = chiType(c);
    Chili d = chiNewFlags(t, s, CHI_NEW_PIN | CHI_NEW_TRY);
    if (chiSuccess(d))
        memcpy(chiRawPayload(d), chiRawPayload(c), CHI_WORDSIZE * s);
    return d;
}

/**
 * Pin object and fail hard if not possible
 */
Chili chiPin(Chili c) {
    Chili pinned = chiTryPin(c);
    if (!chiSuccess(pinned))
        chiErr("Pinning %C failed", c);
    return pinned;
}

/**
 * Strings need special treatment due to the unboxed small
 * string optimization. Chili strings are always zero terminated.
 * However it must still be ensured that the string does not contain inner zero bytes.
 */
Chili chiTryPinString(Chili c) {
    if (!chiTrue(c) || (!chiUnboxed(c) && chiPinned(c)))
        return c;
    ChiStringRef r = chiStringRef(&c);
    Chili pinned = chiStringNewFlags(r.size, CHI_NEW_TRY | CHI_NEW_PIN | CHI_NEW_UNINITIALIZED);
    if (chiSuccess(pinned))
        memcpy(chiRawPayload(pinned), r.bytes, r.size);
    return pinned;
}

ChiWord* chiGrowHeap(ChiWord* hp, const ChiWord* hl) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    // Only grow the heap if really needed
    // There might be spurious calls since HL=0 is used as interrupt trigger
    if (hl > chiNurseryLimit(&proc->nursery)) {
        chiNurseryGrow(&proc->nursery);
        *proc->trigger.hl = proc->trigger.interrupt ? 0 : chiNurseryLimit(&proc->nursery);
        return chiNurseryPtr(&proc->nursery);
    }
    return hp;
}

Chili chiBox64(uint64_t i) {
    Chili c = chiNew(CHI_BOX64, 1);
    *(uint64_t*)chiRawPayload(c) = i;
    return c;
}
