#include "runtime.h"

CHI_INL void blockManagerLock(ChiBlockManager* bm) {
    if (bm->shared)
        chiMutexLock(&bm->mutex);
}

CHI_INL void blockManagerUnlock(ChiBlockManager* bm) {
    if (bm->shared)
        chiMutexUnlock(&bm->mutex);
}

CHI_DEFINE_AUTO_LOCK(ChiBlockManager, blockManagerLock, blockManagerUnlock)
#define CHI_LOCK_BM(m) CHI_AUTO_LOCK(ChiBlockManager, m)

static void poisonBlock(ChiBlock* b, uint8_t poison, size_t blockSize) {
    if (CHI_POISON_BLOCK_ENABLED)
        chiPoisonMem(b->data, poison, (size_t)((uint8_t*)b + blockSize - b->data));
}

static void initBlock(ChiBlockManager* bm, ChiBlock* b) {
    CHI_ASSERT(bm->blockSize % (CHI_WORDSIZE * CHI_WORDBITS) == 0);
    CHI_POISON(b->canary, CHI_POISON_BLOCK_CANARY);
    CHI_POISON(b->owner, bm);
    poisonBlock(b, CHI_POISON_BLOCK_USED, bm->blockSize);
    CHI_ASSERT(bm->blockSize);
    CHI_ASSERT(((uintptr_t)b / bm->blockSize) * bm->blockSize == (uintptr_t)b); // alignment
    CHI_ASSERT(chiBlock(bm, b) == b);
    CHI_ASSERT(chiBlock(bm, (ChiWord*)b + 123) == b);
}

static ChiBlock* freshBlock(ChiBlockManager* bm) {
    ChiWord *blockStart = bm->nextBlock, *blockEnd = blockStart + bm->blockSize / CHI_WORDSIZE;
    ChiBlock* block;
    if (blockStart && blockEnd <= bm->chunkEnd) {
        bm->nextBlock = blockEnd == bm->chunkEnd ? 0 : blockEnd;
        block = (ChiBlock*)blockStart;
    } else {
        ChiChunk* r = chiBlockManagerChunkNew(bm->rt, bm->chunkSize, CHI_MAX(CHI_CHUNK_MIN_SIZE, bm->blockSize));
        bm->chunkEnd = CHI_ALIGN_CAST((uint8_t*)r->start + bm->chunkSize, ChiWord*);
        bm->nextBlock = (ChiWord*)r->start + bm->blockSize / CHI_WORDSIZE;
        chiChunkListAppend(&bm->chunkList, r);
        block = (ChiBlock*)r->start;
    }
    ++bm->blocksCount;
    chiBlockListPoison(block);
    return block;
}

static void freeChunkList(ChiBlockManager* bm) {
    if (bm->blocksCount == bm->freeList.length) {
        while (!chiChunkListNull(&bm->chunkList))
            chiBlockManagerChunkFree(bm->rt, chiChunkListPop(&bm->chunkList));
        chiBlockListInit(&bm->freeList);
        bm->nextBlock = bm->chunkEnd = 0;
        bm->blocksCount = 0;
    }
}

void chiBlockManagerSetup(ChiBlockManager* bm, size_t blockSize, size_t chunkSize, bool shared, ChiRuntime* rt) {
    CHI_ZERO_STRUCT(bm);
    chiChunkListInit(&bm->chunkList);
    chiBlockListInit(&bm->freeList);
    bm->chunkSize = chunkSize;
    bm->blockSize = blockSize;
    bm->blockMask = CHI_ALIGNMASK(blockSize);
    bm->rt = rt;
    bm->shared = shared;
    if (bm->shared)
        chiMutexInit(&bm->mutex);
}

void chiBlockManagerDestroy(ChiBlockManager* bm) {
    CHI_ASSERT(bm->blocksCount == bm->freeList.length);
    freeChunkList(bm);
    if (bm->shared)
        chiMutexDestroy(&bm->mutex);
    CHI_POISON_STRUCT(bm, CHI_POISON_DESTROYED);
}

ChiBlock* chiBlockManagerAlloc(ChiBlockManager* bm) {
    ChiBlock* b;
    {
        CHI_LOCK_BM(bm);
        b = chiBlockListNull(&bm->freeList) ? freshBlock(bm) : chiBlockListPop(&bm->freeList);
    }
    initBlock(bm, b);
    return b;
}

void chiBlockManagerFreeList(ChiBlockManager* bm, ChiBlockList* list) {
    if (chiBlockListNull(list))
        return;

    if (CHI_POISON_ENABLED) {
        CHI_FOREACH_BLOCK_NODELETE (b, list) {
            CHI_ASSERT_POISONED(b->owner, bm);
            poisonBlock(b, CHI_POISON_BLOCK_FREE, bm->blockSize);
        }
    }

    CHI_LOCK_BM(bm);
    chiBlockListJoin(&bm->freeList, list);
    freeChunkList(bm);
}

void chiBlockManagerFree(ChiBlockManager* bm, ChiBlock* b) {
    if (CHI_POISON_ENABLED) {
        CHI_ASSERT_POISONED(b->owner, bm);
        poisonBlock(b, CHI_POISON_BLOCK_FREE, bm->blockSize);
    }
    CHI_LOCK_BM(bm);
    chiBlockListPrepend(&bm->freeList, b);
    freeChunkList(bm);
}

void chiBlockAllocSetup(ChiBlockAlloc* ba, ChiBlockManager* bm, size_t limit) {
    CHI_ZERO_STRUCT(ba);
    ba->manager = bm;
    ba->limit = limit;
    chiBlockListInit(&ba->usedList);
    chiBlockAllocFresh(ba);
}

void chiBlockAllocDestroy(ChiBlockAlloc* ba) {
    chiBlockManagerFreeList(ba->manager, &ba->usedList);
    CHI_POISON_STRUCT(ba, CHI_POISON_DESTROYED);
}

ChiBlock* chiBlockAllocTake(ChiBlockAlloc* ba) {
    ChiBlock* b = chiBlockManagerAlloc(ba->manager);
    b->alloc.ptr = b->alloc.base = chiBlockAllocBase(b, ba->manager->blockSize);
    b->alloc.end = (ChiWord*)b + ba->manager->blockSize / CHI_WORDSIZE;
    memset(&b->alloc.forward, 0, (size_t)((uint8_t*)b->alloc.base - (uint8_t*)b->alloc.forward));
    chiBlockListAppend(&ba->usedList, b);
    if (ba->limit && ba->usedList.length >= ba->limit)
        chiBlockAllocLimitReached(ba->manager);
    return b;
}

ChiBlock* chiBlockAllocFresh(ChiBlockAlloc* ba) {
    return ba->block = chiBlockAllocTake(ba);
}
