#include <chili/cont.h>
#include <setjmp.h>
#include <stdlib.h>
#include "../export.h"
#include "../runtime.h"
#include "../strutil.h"
#include "test.h"

extern TestFn *__start_chi_test_registry[], *__stop_chi_test_registry[];
extern Bench *__start_chi_bench_registry[], *__stop_chi_bench_registry[];

static void shuffle(ChiRandState randState, TestFn** a, size_t n) {
    if (n == 0)
        return;
    for (size_t i = 0; i < n - 1; i++) {
        size_t j = i + (size_t)chiRand(randState) / (~UINT64_C(0) / (n - i) + 1);
        TestFn* t = a[j];
        a[j] = a[i];
        a[i] = t;
    }
}

static uint64_t randSeed;

STATIC_CONT(runBench) {
    PROLOGUE(runBench);
    LIMITS(.interrupt = true); // force safepoint

    uint32_t i = chiToUInt32(A(0));

    enum { REPEAT = 3 };

    uint32_t j = i / REPEAT;
    if (__start_chi_bench_registry + j >= __stop_chi_bench_registry)
        chiExit(0);

    Bench* b = __start_chi_bench_registry[j];
    if (i % REPEAT == 0)
        chiSinkFmt(chiStdout, "%-20s │ %8u runs", b->name, b->runs);

    BenchState state = { .run = 0 };
    chiRandInit(state.rand, randSeed);

    const ChiNanos begin = chiClockMonotonicFine();
    while (state.run < b->runs) {
        b->fn(&state);
        ++state.run;
    }
    const ChiNanos delta = chiNanosDelta(chiClockMonotonicFine(), begin);
    chiSinkFmt(chiStdout, " │ %8t %8t/run", delta, chiNanos(CHI_UN(Nanos, delta) / b->runs));
    if (i % REPEAT == REPEAT - 1)
        chiSinkPutc(chiStdout, '\n');
    chiSinkFlush(chiStdout);

    A(0) = chiFromUInt32(i + 1);
    KNOWN_JUMP(runBench);
}

STATIC_CONT(runTest, .na = 1) {
    PROLOGUE(runTest);
    LIMITS(.interrupt = true); // force safepoint

    uint32_t i = chiToUInt32(A(0));
    uint32_t failed = chiToUInt32(A(1));
    if (__start_chi_test_registry + i >= __stop_chi_test_registry) {
        chiSinkFmt(chiStdout, "\n%ld succeeded, %u failed.\n",
                   (__stop_chi_test_registry - __start_chi_test_registry) - failed, failed);
        chiExit(failed > 0);
    }

    TestState state = { .failed = false };
    chiRandInit(state.rand, randSeed);

    __start_chi_test_registry[i](&state);
    failed += state.failed;
    chiSinkPutc(chiStdout, state.failed ? 'F' : '.');
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
        CHI_IGNORE_RESULT(chiReadUInt64(&randSeed, &end));
    } else {
        randSeed = CHI_UN(Nanos, chiClockMonotonicFine());
    }
    chiSinkFmt(chiStdout, "*** Running test suite with TEST_SEED=%ju ***\n", randSeed);

    ChiRandState randState;
    chiRandInit(randState, randSeed);
    shuffle(randState, __start_chi_test_registry,
            (size_t)(__stop_chi_test_registry - __start_chi_test_registry));

    A(0) = CHI_FALSE;
    A(1) = CHI_FALSE;
    KNOWN_JUMP(runTest);
}

static jmp_buf jmpBuf;
static void jmpExit(int CHI_UNUSED(result)) {
    longjmp(jmpBuf, 1);
}

CHI_WARN_OFF(frame-larger-than=)
int main(int argc, char** argv) {
    chiSystemSetup();

    CHI_AUTO_ALLOC(char*, argvCopy, (size_t)(argc + 1));
    for (int i = 0; i <= argc; ++i)
        argvCopy[i] = argv[i];

    // Runtime reenter test
    ChiRuntime rt;
    if (!setjmp(jmpBuf))
        chiRuntimeMain(&rt, argc, argvCopy, jmpExit);

    chiRuntimeMain(&rt, argc, argv, exit);
}
CHI_WARN_ON
