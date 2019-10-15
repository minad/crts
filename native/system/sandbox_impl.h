#include <libc.h>
#include "../chunk.h"

int sb_main(int argc, char** argv) {
    chiChunkSetup(&(ChiChunkOption){.arenaStart = sb_info->heap.start, .arenaSize = sb_info->heap.size });
    __libc_call_ctors();
    chiMain(argc, argv);
}

ChiNanos chiCondTimedWait(ChiCond* CHI_UNUSED(c), ChiMutex* CHI_UNUSED(m), ChiNanos ns) {
    ChiNanos begin = chiClock(CHI_CLOCK_REAL_FINE);
    sb_yield(CHI_UN(Nanos, ns), &(struct sb_mask){});
    return chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), begin);
}
