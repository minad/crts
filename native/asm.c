#include "asm.h"

CHI_COLD uint32_t chiCpuCyclesOverhead(void) {
    static uint32_t result = 0;
    if (result)
        return result;
    uint32_t min = ~0U;
    for (uint32_t i = 0; i < 50; ++i) {
        uint64_t a, b;
        a = chiCpuCycles(); b = chiCpuCycles(); b = chiCpuCycles(); b = chiCpuCycles(); b = chiCpuCycles();
        b = chiCpuCycles(); b = chiCpuCycles(); b = chiCpuCycles(); b = chiCpuCycles(); b = chiCpuCycles();
        min = CHI_MIN(min, (uint32_t)(b - a));
    }
    return result = min / 10;
}
