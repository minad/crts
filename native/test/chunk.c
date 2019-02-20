#include "test.h"
#include "../chunk.h"

TEST(chunkStress) {
    ChiChunk* chunk[100] = {};
    for (size_t i = 1; i < 10000; ++i) {
        for (size_t j = 0; j < CHI_DIM(chunk); ++j) {
            if (!chunk[j]) {
                size_t s;
                do {
                    s = (size_t)RAND % 10;
                } while (!s);
                size_t t = RAND % 10;
                chunk[j] = chiChunkNew(s * CHI_CHUNK_MIN_SIZE, t ? 1ULL << (t + CHI_CHUNK_MIN_SHIFT) : 0);
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
