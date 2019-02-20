#include <chili/cont.h>
#include <stdlib.h>
#include "../runtime.h"
#include "../export.h"
#include "test.h"
#include "../strutil.h"

extern TestFn *__start_test_registry[], *__stop_test_registry[];
extern Bench *__start_bench_registry[], *__stop_bench_registry[];

static void shuffle(ChiRandState randState, TestFn** a, size_t n) {
    if (n == 0)
        return;
    for (size_t i = 0; i < n - 1; i++) {
        size_t j = i + (size_t)chiRand(randState) / (~0ULL / (n - i) + 1);
        TestFn* t = a[j];
        a[j] = a[i];
        a[i] = t;
    }
}

static uint64_t randSeed;

STATIC_CONT(runBench) {
    PROLOGUE(runBench);
    LIMITS();

    uint32_t i = chiToUInt32(A(0));

    enum { REPEAT = 3 };

    uint32_t j = i / REPEAT;
    if (__start_bench_registry + j >= __stop_bench_registry) {
        A(0) = CHI_FALSE;
        KNOWN_JUMP(chiExit);
    }

    Bench* b = __start_bench_registry[j];
    if (i % REPEAT == 0)
        chiSinkFmt(chiStdout, "%-20s │ %8u runs", b->name, b->runs);

    ChiRandState randState;
    chiRandInit(randState, randSeed);

    const ChiNanos begin = chiClock(CHI_CLOCK_REAL_FINE);
    for (uint32_t run = 0; run < b->runs; ++run)
        b->fn(randState, run);
    const ChiNanos delta = chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), begin);
    chiSinkFmt(chiStdout, " │ %8t %8t/run", delta, (ChiNanos){CHI_UN(Nanos, delta) / b->runs });
    if (i % REPEAT == REPEAT - 1)
        chiSinkPutc(chiStdout, '\n');
    chiSinkFlush(chiStdout);

    A(0) = chiFromUInt32(i + 1);
    KNOWN_JUMP(runBench);
}

STATIC_CONT(runTest,, .na = 1) {
    PROLOGUE(runTest);
    LIMITS();

    uint32_t i = chiToUInt32(A(0));
    uint32_t failed = chiToUInt32(A(1));
    if (__start_test_registry + i >= __stop_test_registry) {
        chiSinkFmt(chiStdout, "\n%ld succeeded, %u failed.\n",
                   (__stop_test_registry - __start_test_registry) - failed, failed);
        A(0) = chiFromBool(failed > 0);
        KNOWN_JUMP(chiExit);
    }

    ChiRandState randState;
    chiRandInit(randState, randSeed);

    bool fail = false;
    __start_test_registry[i](randState, &fail);
    failed += fail;
    chiSinkPutc(chiStdout, fail ? 'F' : '.');
    chiSinkFlush(chiStdout);

    A(0) = chiFromUInt32(i + 1);
    A(1) = chiFromUInt32(failed);
    KNOWN_JUMP(runTest);
}

CONT(z_Main) {
    PROLOGUE(z_Main);

    ChiRuntime* rt = CHI_CURRENT_RUNTIME;
    if (rt->argc == 2 && streq(rt->argv[1], "bench")) {
        A(0) = CHI_FALSE;
        KNOWN_JUMP(runBench);
    }

    if (rt->argc == 2 && strstarts(rt->argv[1], "seed=")) {
        const char* end = rt->argv[1] + 5;
        randSeed = chiReadUInt64(&end);
    } else {
        randSeed = CHI_UN(Nanos, chiClock(CHI_CLOCK_REAL_FINE));
    }
    chiSinkFmt(chiStdout, "*** Running test suite with TEST_SEED=%ju ***\n", randSeed);

    ChiRandState randState;
    chiRandInit(randState, randSeed);
    shuffle(randState, __start_test_registry,
            (size_t)(__stop_test_registry - __start_test_registry));

    A(0) = CHI_FALSE;
    A(1) = CHI_FALSE;
    KNOWN_JUMP(runTest);
}

int main(int argc, char** argv) {
    chiMain(argc, argv);
}
