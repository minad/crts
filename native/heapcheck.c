#include "heapcheck.h"
#include "error.h"
#include "runtime.h"
#include "set.h"
#include "bit.h"
#include "stack.h"
#include <stdlib.h>

typedef struct {
    ChiBlockVec  stack;
    Set          visited;
    Set          gray;
    Set          dirty;
    ChiMSPhase   msPhase;
    ChiHeapCheck mode;
    uintptr_t    blockMask;
    uint32_t     heapGen;
    uint32_t     depth;
} CheckState;

#define FAIL(cond, ...)                         \
    ({                                          \
        if (!(cond))                            \
            chiAbort(__VA_ARGS__);              \
    })

static bool isDirty(CheckState* state, Chili c) {
    if (chiMajor(c))
        return chiObjectDirty(chiObject(c));
    void* payload = chiRawPayload(c);
    ChiBlock* block = chiBlock(payload, state->blockMask);
    return chiBitGet(block->marked, chiBlockIndex(block, payload));
}

static void checkScan(CheckState*, Chili);

static void checkObject(CheckState* state, Chili c) {
    FAIL(chiReference(c), "Not a reference %C", c);

    if (!setInserted(&state->visited, c))
        return;

    const ChiType type = chiType(c);
    if (chiMajor(c)) {
        ChiColor color = chiObjectColor(chiObject(c));

        FAIL(!chiObjectActive(chiObject(c)), "Found active object %C", c);

        if (state->mode == CHI_HEAPCHECK_PHASECHANGE) {
            switch (state->msPhase) {
            case CHI_MS_PHASE_IDLE:
                if (chiRaw(type)) {
                    FAIL(color != CHI_GRAY,
                         "Found gray raw major %C instead of white/black before mark", c);
                } else {
                    FAIL(color != CHI_BLACK,
                         "Found black major %C instead of white/gray before mark", c);
                }
                break;
            case CHI_MS_PHASE_MARK:
                FAIL(color == CHI_BLACK,
                     "Found major %C with color %u instead of black after mark",
                     c, color);
                break;
            case CHI_MS_PHASE_SWEEP:
                FAIL(color == CHI_WHITE,
                     "Found major %C with color %u instead of white after sweep",
                     c, color);
                break;
            }
        }

        FAIL(color != CHI_GRAY || setFind(&state->gray, c), "Gray object %C not found in gray list", c);
    }

    FAIL(chiMajor(c) || chiGen(c, state->blockMask) < state->heapGen, "Invalid generation found %C", c);
    FAIL(!isDirty(state, c) || setFind(&state->dirty, c), "Object marked dirty, but not in dirty set: %C", c);

    if (!chiRaw(type)) { // Raw objects don't need scanning
        if (state->depth > 0) {
            --state->depth;
            checkScan(state, c);
            ++state->depth;
        } else {
            chiBlockVecPush(&state->stack, c);
        }
    }
}

static void checkRef(CheckState* state, Chili src, Chili c) {
    if (!chiReference(c))
        return;

    if (state->msPhase != CHI_MS_PHASE_SWEEP && chiMajor(c) && chiMajor(src) &&
        chiObjectColor(chiObject(src)) == CHI_BLACK) {
        FAIL(chiObjectColor(chiObject(c)) != CHI_WHITE, "Found major black pointing to white object: %C -> %C", src, c);
    }

    if (chiGen(src, state->blockMask) > chiGen(c, state->blockMask))
        FAIL(isDirty(state, src), "Heap invariant violated: %C -> %C", src, c);
}

static void checkScan(CheckState* state, Chili c) {
    FAIL(chiReference(c) && !chiRaw(chiType(c)), "Invalid object enqueued %C", c);

    const ChiType type = chiType(c);
    if (type != CHI_STACK) {
        FAIL(type <= CHI_LAST_IMMUTABLE
             || type == CHI_ARRAY || type == CHI_THREAD
             || type == CHI_THUNK || type == CHI_STRINGBUILDER,
             "Invalid object type found");
        for (Chili* payload = chiPayload(c), *p = payload, *end = p + chiSize(c); p < end; ++p)
            checkRef(state, c, *p);
    } else {
        ChiStack* stack = chiToStack(c);
        for (Chili* p = stack->base; p < stack->sp; ++p)
            checkRef(state, c, *p);
    }
}

void chiHeapCheck(ChiRuntime* rt, ChiHeapCheck mode) {
    ChiMSPhase phase = rt->gc.msPhase;
    const ChiRuntimeOption* opt = &rt->option;

    CheckState state = {
        .msPhase   = phase,
        .mode      = mode,
        .blockMask = CHI_MASK(opt->block.size),
        .heapGen   = opt->block.gen,
        .depth     = opt->heap.scanDepth,
    };
    chiBlockVecInit(&state.stack, &rt->blockTemp);

    CHI_FOREACH_PROCESSOR (proc, rt) {
        for (uint32_t gen = 0; gen < state.heapGen; ++gen) {
            CHI_BLOCKVEC_FOREACH(c, proc->gcPS.dirty + gen) {
                if (gen) {
                    FAIL(chiGen(c, state.blockMask) == gen,
                             "Generation of object %C in dirty list is wrong", c);
                    Chili* payload = chiPayload(c);
                    ChiBlock* block = chiBlock(payload, state.blockMask);
                    size_t idx = chiBlockIndex(block, payload);
                    FAIL(chiBitGet(block->marked, idx), "Object %C is not marked dirty", c);
                } else {
                    FAIL(chiMajor(c), "Generation of object %C in dirty list is wrong", c);
                    FAIL(chiObjectDirty(chiObject(c)), "Object %C is not marked dirty", c);
                }
                FAIL(setInserted(&state.dirty, c), "Dirty set contains duplicate %C", c);
            }
        }
    }

    CHI_FOREACH_PROCESSOR (proc, rt) {
        ChiBlockVec* grayList = &proc->gcPS.grayList;
        FAIL(phase != CHI_MS_PHASE_IDLE  || chiBlockVecNull(grayList), "Non-empty gray list in idle phase");
        FAIL(phase != CHI_MS_PHASE_SWEEP || chiBlockVecNull(grayList), "Non-empty gray list in sweep phase");
        CHI_BLOCKVEC_FOREACH(c, grayList) {
            ChiColor color = chiObjectColor(chiObject(c));
            FAIL(color != CHI_WHITE, "Gray list contains white object %C", c);
            setInserted(&state.gray, c);
        }
    }

    for (size_t i = 0; i < rt->gc.roots.used; ++i) {
        Chili root = rt->gc.roots.elem[i];
        FAIL(chiMajor(root), "Roots must live in major heap %C", root);
        checkObject(&state, root);
    }

    CHI_FOREACH_PROCESSOR (proc, rt) {
        checkObject(&state, proc->thread);
        checkObject(&state, proc->schedulerThread);
    }

    Chili c;
    while (chiBlockVecPop(&state.stack, &c))
        checkScan(&state, c);

    setDestroy(&state.visited);
    setDestroy(&state.gray);
    setDestroy(&state.dirty);
}
