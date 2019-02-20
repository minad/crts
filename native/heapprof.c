#include "heapprof.h"
#include "blockvec.h"
#include "set.h"
#include "stack.h"
#include "runtime.h"
#include "thread.h"
#include <chili/object/thunk.h>

typedef struct {
    ChiBlockVec stack;
    Set         visited;
    ChiHeapProf prof;
    uint32_t    depth;
} ProfState;

static void countObject(ProfState*, Chili);

static void countChili(ProfState* state, Chili c) {
    if (chiReference(c) && setInserted(&state->visited, c)) {
        if (state->depth > 0) {
            --state->depth;
            countObject(state, c);
            ++state->depth;
        } else {
            chiBlockVecPush(&state->stack, c);
        }
    }
}

static void countObject(ProfState* state, Chili c) {
    size_t size = chiSize(c);
    ChiType type = chiType(c);
    ChiObjectCount* count;
    switch (type) {
    case CHI_RAW: count = &state->prof.raw; break;
    case CHI_BIGINT: count = &state->prof.bigint; break;
    case CBY_FFI: count = &state->prof.ffi; break;
    case CHI_BUFFER: count = &state->prof.buffer; break;
    case CHI_STRING: count = &state->prof.string; break;
    case CHI_BOX64: count = &state->prof.box64; break;
    case CHI_STRINGBUILDER:
        count = &state->prof.stringbuilder;
        countChili(state, chiAtomicLoad(&chiToStringBuilder(c)->string));
        break;
    case CHI_THUNK:
        count = &state->prof.thunk;
        countChili(state, chiAtomicLoad(&chiToThunk(c)->val));
        break;
    case CHI_THREAD: {
        count = &state->prof.thread;
        ChiThread* t = chiToThread(c);
        countChili(state, chiThreadName(c));
        countChili(state, chiAtomicLoad(&t->stack));
        countChili(state, chiThreadState(c));
        countChili(state, chiThreadLocal(c));
        break;
    }
    case CHI_STACK: {
        count = &state->prof.stack;
        ChiStack* stack = chiToStack(c);
        for (Chili* p = stack->base; p < stack->sp; ++p)
            countChili(state, *p);
        break;
    }
    case CHI_ARRAY:
    case CHI_FIRST_TAG...CHI_LAST_IMMUTABLE:
    default:
        count = chiFn(type) || type == CHI_THUNKFN ? &state->prof.fn
            : type == CHI_ARRAY ? &state->prof.array
            : &state->prof.data;
        for (Chili* payload = chiPayload(c), *p = payload, *end = p + size; p < end; ++p)
            countChili(state, *p);
        break;
    }
    chiCountObject(count, size);
}

void chiHeapProf(ChiRuntime* rt, ChiHeapProf* prof) {
    ProfState state = { .depth = rt->option.heap.scanDepth };
    chiBlockVecInit(&state.stack, &rt->blockTemp);

    for (size_t i = 0; i < rt->gc.roots.used; ++i)
        countChili(&state, rt->gc.roots.elem[i]);

    CHI_FOREACH_PROCESSOR (proc, rt) {
        countChili(&state, proc->thread);
        countChili(&state, proc->schedulerThread);
    }

    Chili c;
    while (chiBlockVecPop(&state.stack, &c))
        countObject(&state, c);

    setDestroy(&state.visited);
    *prof = state.prof;
}
