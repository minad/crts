#include "chunk.h"
#include "runtime.h"

static void blockPoison(ChiBlock* b, uint8_t poison) {
    chiPoisonBlock(b->base, poison, CHI_WORDSIZE * (size_t)(b->end - b->base));
}

static void initBlock(ChiBlockManager* bm, ChiBlock* b, uint32_t markedBits) {
    CHI_ASSERT(bm->blockSize % (CHI_WORDSIZE * CHI_WORDBITS) == 0);
    CHI_POISON(b->canary, CHI_POISON_BLOCK_CANARY);
    b->ptr = b->base = CHI_ALIGN_CAST((uint8_t*)b->marked, ChiWord*) + markedBits * bm->blockSize / (CHI_WORDSIZE * CHI_WORDBITS);
    b->end = (ChiWord*)b + bm->blockSize / CHI_WORDSIZE;
    b->gen = CHI_GEN_NURSERY;
    memset(&b->marked, 0, (size_t)((uint8_t*)b->base - b->marked));
    blockPoison(b, CHI_POISON_BLOCK_USED);
}

static ChiBlock* freshBlock(ChiBlockManager* bm) {
    ChiWord *blockStart = bm->nextBlock, *blockEnd = blockStart + bm->blockSize / CHI_WORDSIZE;

    ChiBlock* block;
    if (blockStart && blockEnd <= bm->chunkEnd) {
        bm->nextBlock = blockEnd == bm->chunkEnd ? 0 : blockEnd;
        block = (ChiBlock*)blockStart;
    } else {
        ChiChunk* r = chiBlockManagerChunkNew(bm, bm->chunkSize, CHI_MAX(CHI_CHUNK_MIN_SIZE, bm->blockSize));
        bm->chunkEnd = CHI_ALIGN_CAST((char*)r->start + bm->chunkSize, ChiWord*);
        bm->nextBlock = (ChiWord*)r->start + bm->blockSize / CHI_WORDSIZE;
        chiListAppend(&bm->chunkList, &r->list);
        block = (ChiBlock*)r->start;
    }
    chiListPoison(&block->list);
    return block;
}

static void freeChunkList(ChiBlockManager* bm) {
    if (!bm->blocksUsed) {
        while (!chiListNull(&bm->chunkList))
            chiBlockManagerChunkFree(bm, chiChunkPop(&bm->chunkList));
        chiBlockListInit(&bm->free);
        bm->nextBlock = bm->chunkEnd = 0;
    }
}

void chiBlockManagerSetup(ChiBlockManager* bm, size_t blockSize, size_t chunkSize, size_t limit) {
    CHI_CLEAR(bm);
    chiListInit(&bm->chunkList);
    chiBlockListInit(&bm->free);
    chiMutexInit(&bm->mutex);
    bm->chunkSize = chunkSize;
    bm->blockSize = blockSize;
    bm->blocksLimit = limit;
}

void chiBlockManagerDestroy(ChiBlockManager* bm) {
    CHI_ASSERT(!bm->blocksUsed);
    freeChunkList(bm);
    chiMutexDestroy(&bm->mutex);
    chiPoisonMem(bm, CHI_POISON_DESTROYED, sizeof (ChiBlockManager));
}

ChiBlock* chiBlockManagerAlloc(ChiBlockManager* bm, uint32_t markedBits) {
    ChiBlock* b;
    {
        CHI_LOCK(&bm->mutex);
        b = bm->free.count ? chiBlockListPop(&bm->free) : freshBlock(bm);
        ++bm->blocksUsed;
    }
    if (CHI_UNLIKELY(bm->blocksLimit && bm->blocksUsed > bm->blocksLimit))
        chiBlockManagerLimitReached(bm);
    initBlock(bm, b, markedBits);
    CHI_ASSERT(bm->blockSize);
    CHI_ASSERT(((uintptr_t)b / bm->blockSize) * bm->blockSize == (uintptr_t)b); // alignment
    CHI_ASSERT(chiBlock(b, CHI_MASK(bm->blockSize)) == b);
    CHI_ASSERT(chiBlock((ChiWord*)b + 123, CHI_MASK(bm->blockSize)) == b);
    return b;
}

void chiBlockManagerFreeList(ChiBlockManager* bm, ChiBlockList* list) {
    if (chiBlockListNull(list))
        return;

    if (CHI_POISON_ENABLED) {
        CHI_FOREACH_BLOCK_NODELETE (block, list)
            blockPoison(block, CHI_POISON_BLOCK_FREE);
    }

    CHI_LOCK(&bm->mutex);
    CHI_ASSERT(bm->blocksUsed >= list->count);
    bm->blocksUsed -= list->count;
    chiBlockListJoin(&bm->free, list);
    freeChunkList(bm);
}

void chiBlockManagerFree(ChiBlockManager* bm, ChiBlock* block) {
    if (CHI_POISON_ENABLED)
        blockPoison(block, CHI_POISON_BLOCK_FREE);
    CHI_LOCK(&bm->mutex);
    CHI_ASSERT(bm->blocksUsed >= 1);
    --bm->blocksUsed;
    chiBlockListPrepend(&bm->free, block);
    freeChunkList(bm);
}

ChiBlock* chiBlockVecGrow(ChiBlockVec* bv) {
    ChiBlock* b = chiBlockManagerAlloc(bv->manager, 0);
    chiBlockListPrepend(&bv->list, b);
    return b;
}

void chiBlockVecFree(ChiBlockVec* bv) {
    chiBlockManagerFreeList(bv->manager, &bv->list);
}

void chiBlockVecShrink(ChiBlockVec* bv) {
    chiBlockManagerFree(bv->manager, chiBlockListPop(&bv->list));
}

void chiBlockAllocSetup(ChiBlockAlloc* ba, ChiGen gen, ChiBlockManager* bm) {
    CHI_CLEAR(ba);
    ba->gen = gen;
    ba->manager = bm;
    chiBlockListInit(&ba->used);
    chiBlockListInit(&ba->old);
    chiBlockFresh(ba);
}

void chiBlockAllocDestroy(ChiBlockAlloc* ba) {
    CHI_ASSERT(!ba->old.count);
    chiBlockManagerFreeList(ba->manager, &ba->used);
    chiPoisonMem(ba, CHI_POISON_DESTROYED, sizeof (ChiBlockAlloc));
}

ChiBlock* chiBlockFresh(ChiBlockAlloc* ba) {
    ChiBlock* b = ba->block = chiBlockManagerAlloc(ba->manager, 2); // 2 marked bits needed to store forward bit and dirty bit
    b->gen = ba->gen;
    chiBlockListAppend(&ba->used, b);
    return b;
}
