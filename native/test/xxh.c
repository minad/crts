#if 0
#include "test.h"
#include "../xxh.h"
#include "../mem.h"
#define XXH_INLINE_ALL
#include "../xxhash/xxhash.h"

TEST(xxh) {
    for (uint32_t i = 0; i < 1000; ++i) {
        size_t size = (size_t)RAND % 1024;
        CHI_AUTO_ALLOC(uint8_t, buf, size);
        for (size_t j = 0; j < size; ++j)
            buf[j] = (uint8_t)RAND;
        ASSERTEQ(xxh32(buf, size, 0), XXH32(buf, size, 0));
        ASSERTEQ(xxh64(buf, size, 0), XXH64(buf, size, 0));
    }
}
#endif
