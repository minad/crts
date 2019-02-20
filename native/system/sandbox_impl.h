#include "../mem.h"
#include "../chunk.h"
#include <bits/ctors.h>

void* chiTaskLocal(size_t size, ChiDestructor CHI_UNUSED(destructor)) {
    return chiAlloc(size);
}

int sb_main(int argc, char** argv) {
    chiChunkSetup(&(ChiChunkOption){.arenaStart = sb_info->heap.start, .arenaSize = sb_info->heap.size });
    __call_ctors();
    chiMain(argc, argv);
}

ChiNanos chiCondTimedWait(ChiCond* CHI_UNUSED(c), ChiMutex* CHI_UNUSED(m), ChiNanos ns) {
    ChiNanos begin = chiClock(CHI_CLOCK_REAL_FINE);
    sb_yield(CHI_UN(Nanos, ns), &(struct sb_mask){});
    return chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), begin);
}
