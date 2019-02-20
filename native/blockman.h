#pragma once

#include "system.h"
#include "block.h"

typedef struct ChiChunk_ ChiChunk;
typedef struct ChiBlockManager_ ChiBlockManager;

/**
 * A block manager manages equally sized ChiBlock%s
 * which are allocated from the chunk manager.
 * The block and chunk sizes can be modified
 * via the runtime options.
 *
 * @note The block manager is thread safe, however
 *       for low contention each ChiProcessor uses its own block managers
 *       for the nursery allocations.
 */
struct ChiBlockManager_ {
    ChiBlockList free;
    size_t       blocksUsed, blocksLimit;
    size_t       blockSize, chunkSize;
    ChiWord*     nextBlock;
    ChiWord*     chunkEnd;
    ChiList      chunkList;
    ChiMutex     mutex;
};

void chiBlockManagerSetup(ChiBlockManager*, size_t, size_t, size_t);
void chiBlockManagerDestroy(ChiBlockManager*);
CHI_WU CHI_RET_NONNULL ChiBlock* chiBlockManagerAlloc(ChiBlockManager*, uint32_t);
void chiBlockManagerFree(ChiBlockManager*, ChiBlock*);
void chiBlockManagerFreeList(ChiBlockManager*, ChiBlockList*);

// call outs
void chiBlockManagerLimitReached(ChiBlockManager*);
CHI_WU ChiChunk* chiBlockManagerChunkNew(ChiBlockManager*, size_t, size_t);
void chiBlockManagerChunkFree(ChiBlockManager*, ChiChunk*);
