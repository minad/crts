#include "copy.h"
#include "error.h"
#include "event.h"
#include "new.h"
#include "promote.h"
#include "runtime.h"

static Chili heapAllocFailed(ChiProcessor* proc, ChiType type, size_t size) {
    chiEvent(proc, HEAP_ALLOC_FAILED, .type=chiStringRef(chiTypeName(type)), .size=size);
    return CHI_FAIL;
}

CHI_NOINL Chili chiNewMajorClean(ChiProcessor* proc, ChiType type, size_t size, ChiNewFlags flags) {
    CHI_ASSERT(!!(flags & CHI_NEW_CLEAN));
    void* p = chiHeapHandleNew(&proc->handle, size, flags);
    if (CHI_UNLIKELY(!p))
        return heapAllocFailed(proc, type, size);
    Chili c = chiWrapLarge(p, size, type);
    CHI_ASSERT(!chiObjectDirty(chiObject(c)));
    return c;
}

CHI_NOINL Chili chiNewMajorDirty(ChiProcessor* proc, ChiType type, size_t size, ChiNewFlags flags) {
    CHI_ASSERT(!(flags & CHI_NEW_CLEAN));
    void* p = chiHeapHandleNew(&proc->handle, size, flags);
    if (CHI_UNLIKELY(!p))
        return heapAllocFailed(proc, type, size);
    // Non raw objects are marked dirty, since object will be modified after allocation
    // In particular the object will be initialized with gen=0 references, violating the generational invariant!
    Chili c = chiWrapLarge(p, size, type);
    CHI_ASSERT(chiObjectDirty(chiObject(c)));
    chiBlockVecPush(&proc->heap.majorDirty, c);
    return c;
}

Chili chiNewFlags(ChiType type, size_t size, ChiNewFlags flags) {
    return chiNewInl(CHI_CURRENT_PROCESSOR, type, size, flags);
}

/**
 * Allocate new object in an out-of-line block.
 * Usually bump allocation should be preferred.
 * If the object is large it will be directly allocated on the major heap.
 */
Chili chiNew(ChiType type, size_t size) {
    return chiNewFlags(type, size, CHI_NEW_DEFAULT);
}

ChiWord* chiGrowHeap(ChiWord* hp, const ChiWord* hl) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    // Only grow the heap if really needed
    // There might be spurious calls since HL=0 is used as interrupt trigger
    if (hl > chiLocalHeapBumpLimit(&proc->heap)) {
        chiLocalHeapBumpGrow(&proc->heap);
        *proc->reg.hl = chiTriggered(&proc->trigger.interrupt) ? 0 : chiLocalHeapBumpLimit(&proc->heap);
        return chiLocalHeapBumpBase(&proc->heap);
    }
    return hp;
}

Chili chiBoxNew(ChiWord v) {
    Chili c = chiNew(CHI_BOX, 1);
    chiToBox(c)->value = v;
    return c;
}
