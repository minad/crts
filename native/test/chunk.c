#include "../chunk.h"
#include "test.h"

TEST(chunkStress) {
    ChiChunk* chunk[100] = {};
    for (size_t i = 1; i < 10000; ++i) {
        for (size_t j = 0; j < CHI_DIM(chunk); ++j) {
            if (!chunk[j]) {
                size_t s;
                do {
                    s = (size_t)RAND % 10;
                } while (!s);
                s *= CHI_CHUNK_MIN_SIZE;
                size_t a = RAND % 10;
                if (a) {
                    a = UINT64_C(1) << (a + CHI_CHUNK_MIN_SHIFT);
                    while (a > s)
                        a >>= 1;
                }
                chunk[j] = chiChunkNew(s, a);
            }
        }
        for (size_t j = 0; j < CHI_DIM(chunk); ++j) {
            if (RAND % 1) {
                chiChunkFree(chunk[j]);
                chunk[j] = 0;
            }
        }
    }
    for (size_t j = 0; j < CHI_DIM(chunk); ++j)
        chiChunkFree(chunk[j]);
}
