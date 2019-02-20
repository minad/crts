#include "test.h"
#include "../mem.h"

TEST(chiAlloc) {
    //ASSERT(!chiAlloc(0));
    chiFree(0);

    for (size_t run = 0; run < 10; ++run) {
        uint8_t* mem[64];
        uint16_t size[64];
        uint8_t byte[64];
        for (size_t i = 0; i < CHI_DIM(mem); ++i) {
            byte[i] = (uint8_t)RAND;
            size[i] = (uint16_t)RAND;
            mem[i] = (uint8_t*)chiAlloc(size[i]);
            for (size_t j = 0; j < size[i]; ++j)
                mem[i][j] = (uint8_t)(byte[i] + j);
        }
        for (size_t i = 0; i < CHI_DIM(mem); ++i) {
            for (size_t j = 0; j < size[i]; ++j)
                ASSERTEQ(mem[i][j], (uint8_t)(byte[i] + j));
            chiFree(mem[i]);
        }
    }
}

TEST(chiZalloc) {
    //ASSERT(!chiZalloc(0));
    uint8_t* mem = (uint8_t*)chiZalloc(123);
    for (size_t j = 0; j < 123; ++j)
        ASSERTEQ(mem[j], 0);
    chiFree(mem);
}

TEST(chiRealloc) {
    //ASSERT(!chiRealloc(0, 0));
    ASSERT(!chiRealloc(chiAlloc(123), 0));

    for (size_t run = 0; run < 10; ++run) {
        uint8_t* mem[64];
        uint16_t size[64];
        uint8_t byte[64];
        for (size_t i = 0; i < CHI_DIM(mem); ++i) {
            byte[i] = (uint8_t)RAND;
            size[i] = (size_t)RAND & UINT16_MAX;
            mem[i] = (uint8_t*)chiAlloc(size[i]);
            for (size_t j = 0; j < size[i]; ++j)
                mem[i][j] = (uint8_t)(byte[i] + j);
        }
        for (size_t i = 0; i < CHI_DIM(mem); ++i) {
            for (size_t j = 0; j < size[i]; ++j)
                ASSERTEQ(mem[i][j], (uint8_t)(byte[i] + j));
            byte[i] = (uint8_t)RAND;
            size[i] = (uint16_t)RAND;
            mem[i] = (uint8_t*)chiRealloc(mem[i], size[i]);
            for (size_t j = 0; j < size[i]; ++j)
                mem[i][j] = (uint8_t)(byte[i] + j);
        }
        for (size_t i = 0; i < CHI_DIM(mem); ++i) {
            for (size_t j = 0; j < size[i]; ++j)
                ASSERTEQ(mem[i][j], (uint8_t)(byte[i] + j));
            chiFree(mem[i]);
        }
    }
}
