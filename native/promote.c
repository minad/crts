#include "copy.h"
#include "new.h"
#include "promote.h"
#include "runtime.h"
#include "tracepoint.h"

static Chili findForwarded(ChiProcessor* proc, Chili c) {
    CHI_BLOCKVEC_FOREACH(ChiPromotedVec, p, &proc->heap.promoted.vec) {
        if (chiIdentical(p->from, c))
            return p->to;
    }
    CHI_BUG("Promoted object not found");
}

CHI_INL Chili newPromoted(ChiHeapHandle* handle, Chili c, ChiNewFlags flags, ChiPromoteStats* stats) {
    size_t size = chiSize(c);
    CHI_NOWARN_UNUSED(stats);
    chiCountObject(stats->copied, size);

    ChiType type = chiType(c);
    CHI_ASSERT(type != CHI_THREAD); // Thread should already be in major-local
    CHI_ASSERT(type != CHI_ARRAY); // Array should already be in major-local
    CHI_ASSERT(type < CHI_BUFFER0); // Buffer should already be in major-local

    void *dstPayload = chiHeapHandleNewSmall(handle, size, flags);
    Chili d = chiWrap(dstPayload, size, type, CHI_GEN_MAJOR);

    // Install thunk forwarding
    Chili thunkVal;
    if (type == CHI_THUNK && !chiThunkForced(c, &thunkVal)) {
        chiFieldInit(&chiToThunk(d)->val, *(Chili*)chiRawPayload(c));
        Chili* fwPayload = (Chili*)chiHeapHandleNewSmall(handle, 2, CHI_NEW_LOCAL | CHI_NEW_CLEAN);
        Chili fw = chiWrap(fwPayload, 2, CHI_THUNK_FN, CHI_GEN_MAJOR);
        chiInit(fw, fwPayload, chiFromCont(&chiThunkForward));
        chiInit(fw, fwPayload + 1, d);
        chiFieldInit(&chiToThunk(c)->val, fw);
        chiCount(stats->thunk, 1);
    } else {
        chiCopyWords(dstPayload, chiRawPayload(c), size);
    }

    return d;
}

#if CHI_SYSTEM_HAS_TASK
static void restoreForwarded(ChiPromoteState* ps) {
    if (!ps->forward)
        return;
    CHI_BLOCKVEC_FOREACH(ChiPromotedVec, p, &ps->proc->heap.promoted.vec) {
        Chili* from = (Chili*)chiRawPayload(p->from);
        CHI_SWAP(*from, p->to);
        CHI_ASSERT(chiGen(p->from) != CHI_GEN_MAJOR);
        CHI_ASSERT(chiGen(p->to) == CHI_GEN_MAJOR);
    }
    chiProcessorRequest(ps->proc, CHI_REQUEST_SCAVENGE);
}

static void copyForwarded(ChiPromoteState* ps) {
    if (ps->forward)
        return;
    ps->forward = true;
    CHI_BLOCKVEC_FOREACH(ChiPromotedVec, p, &ps->proc->heap.promoted.vec) {
        CHI_ASSERT(chiGen(p->from) != CHI_GEN_MAJOR);
        CHI_ASSERT(chiGen(p->to) == CHI_GEN_MAJOR);
        Chili* from = (Chili*)chiRawPayload(p->from);
        CHI_SWAP(*from, p->to);
    }
}

static void rememberForwarded(ChiPromoteState* ps, Chili c, Chili d) {
    ChiPromoted p = { .from = c, .to = d };
    if (ps->forward) {
        Chili* from = (Chili*)chiRawPayload(c);
        CHI_SWAP(*from, p.to);
    }
    chiPromotedVecPush(&ps->proc->heap.promoted.vec, p);
}

static void promoteSharedScan(ChiPromoteState* ps, Chili c) {
    // Raw objects must not be scanned
    CHI_ASSERT(!chiRaw(chiType(c)));

    // Stacks are never shared
    CHI_ASSERT(chiType(c) != CHI_STACK);

    // Threads are always allocated shared
    CHI_ASSERT(chiType(c) != CHI_THREAD);

    // StringBuilder is mutable and the library ensures
    // that StringBuilder objects are never shared.
    CHI_ASSERT(chiType(c) != CHI_STRINGBUILDER);
    CHI_ASSERT(chiType(c) != CHI_PRESTRING);

    size_t size = chiSize(c);
    chiCountObject(ps->stats.scanned, size);
    for (Chili* p = chiPayload(c), *end = p + size; p < end; ++p) {
        if (!chiUnboxed(*p))
            chiPromoteSharedObject(ps, p);
    }
}

void chiPromoteSharedObject(ChiPromoteState* ps, Chili* cp) {
    Chili c = *cp;

    CHI_ASSERT(!chiUnboxed(c));
    CHI_ASSERT(chiType(c) != CHI_STACK);

    if (chiGen(c) != CHI_GEN_MAJOR) {
        if (chiBlockAllocSetForwardMasked(chiRawPayload(c), ps->blockMask)) {
            Chili d = newPromoted(&ps->proc->handle, c, CHI_NEW_SHARED | CHI_NEW_CLEAN,
                                  CHI_AND(CHI_COUNT_ENABLED, &ps->stats));
            rememberForwarded(ps, c, d);
            *cp = d;
            if (chiRaw(chiType(c)))
                return;
            c = d;
            goto scan;
        }

        CHI_ASSERT((findForwarded(ps->proc, c), true));
        copyForwarded(ps);
        *cp = c = *(Chili*)chiRawPayload(c);
        CHI_ASSERT(chiGen(c) == CHI_GEN_MAJOR);
    }

    ChiObject* obj = chiObject(c);
    if (chiObjectShared(obj))
        return; // Object already shared, no scanning necessary

    // Mark object as shared and clean
    atomic_store_explicit(&obj->flags, _CHI_OBJECT_SHARED, memory_order_relaxed);

 scan:
    CHI_ASSERT(!chiRaw(chiType(c)));
    if (ps->depth > 0) {
        --ps->depth;
        promoteSharedScan(ps, c);
        ++ps->depth;
    } else {
        chiBlockVecPush(&ps->stack, c);
    }
}

void chiPromoteSharedEnd(ChiPromoteState* ps) {
    Chili c;
    while (chiBlockVecPop(&ps->stack, &c))
        promoteSharedScan(ps, c);

    restoreForwarded(ps);

    ChiPromoteStats* s = CHI_AND(CHI_COUNT_ENABLED, &ps->proc->heap.promoted.stats);
    CHI_NOWARN_UNUSED(s);
    chiCountAdd(s->copied, ps->stats.copied);
    chiCountAdd(s->scanned, ps->stats.scanned);
    chiCount(s->thunk, ps->stats.thunk);

    CHI_TTP(ps->proc, .name = "runtime;promote");
}

void chiPromoteSharedBegin(ChiPromoteState* ps, ChiProcessor* proc) {
    *ps = (ChiPromoteState){ .proc = proc,
                             .depth = proc->rt->option.heap.scanDepth,
                             .blockMask = proc->heap.manager.blockMask };
    chiBlockVecInit(&ps->stack, &proc->heap.manager);
}
#endif

void chiPromoteShared(ChiProcessor* proc, Chili* cp) {
    ChiPromoteState ps;
    chiPromoteSharedBegin(&ps, proc);
    chiPromoteSharedObject(&ps, cp);
    chiPromoteSharedEnd(&ps);
}

void chiPromoteLocal(ChiProcessor* proc, Chili* cp) {
    Chili c = *cp;
    if (chiGen(c) == CHI_GEN_MAJOR)
        return;

    ChiLocalHeap* h = &proc->heap;

    if (chiBlockAllocSetForwardMasked(chiRawPayload(c), h->manager.blockMask)) {
        bool raw = chiRaw(chiType(c));
        Chili d = newPromoted(&proc->handle, c, raw ? (CHI_NEW_SHARED | CHI_NEW_CLEAN) : CHI_NEW_LOCAL,
                              CHI_AND(CHI_COUNT_ENABLED, &h->promoted.stats));
        if (!raw)
            chiBlockVecPush(&h->majorDirty, d);
        chiPromotedVecPush(&h->promoted.vec, (ChiPromoted){ .from = c, .to = d });
        *cp = d;
    } else {
        // Promoting object again - it is timer for the scavenger!
        chiProcessorRequest(proc, CHI_REQUEST_SCAVENGE);
        *cp = findForwarded(proc, c);
    }
}
