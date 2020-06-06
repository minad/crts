#pragma once

#include "chunk.h"
#include "system.h"

typedef struct ChiBlockManager_ ChiBlockManager;
typedef struct ChiRuntime_ ChiRuntime;
typedef struct ChiBlockLink_ ChiBlockLink;
typedef struct ChiBlockManager_ ChiBlockManager;
struct ChiBlockLink_ { ChiBlockLink *prev, *next; };

#define CHI_FOREACH_BLOCK(block, blocklist)          CHI_LIST_FOREACH(ChiBlock, link, block, blocklist)
#define CHI_FOREACH_BLOCK_NODELETE(block, blocklist) CHI_LIST_FOREACH_NODELETE(ChiBlock, link, block, blocklist)

/**
 * Memory block used by ChiBlockManager and ChiBlockAlloc
 */
typedef struct CHI_WORD_ALIGNED {
    CHI_IF(CHI_POISON_ENABLED, ChiBlockManager* owner; CHI_IF(CHI_ARCH_32BIT, uint32_t _pad;) uint64_t canary;)
    ChiBlockLink link;     ///< Linked list of blocks
    union {
        uint8_t data[0];
        struct {
            ChiWord   *base;      ///< Base pointer
            ChiWord   *end;       ///< Pointer to end of block
            ChiWord   *ptr;       ///< Current allocation pointer
            CHI_IF(CHI_ARCH_32BIT, uint32_t _pad[5];)
            ChiWord   forward[0]; ///< Forwarding bits
        } alloc; // used by allocator
        struct {
            uint8_t* ptr;
            uint8_t* end;
            uint8_t  base[0];
        } vec; // used by block vector
    };
} ChiBlock;

#define LIST_LENGTH
#define LIST_PREFIX chiBlockList
#define LIST_ELEM   ChiBlock
#include "generic/list.h"

_Static_assert(CHI_SIZEOF_WORDS(ChiBlock) // header
               + CHI_CHOICE(CHI_POISON_ENABLED, 0, 2) // header with poisoning is larger
               + CHI_BLOCK_MINSIZE / 64 // forward bits
               + CHI_BLOCK_MINLIMIT     // usable memory
               == CHI_BLOCK_MINSIZE, "Wrong block limit");

_Static_assert(offsetof(ChiBlock, alloc.forward) % CHI_WORDSIZE == 0, "Forward bits should be aligned");

/**
 * A block manager manages equally sized ChiBlock%s
 * which are allocated from the chunk manager.
 * The block and chunk sizes can be modified
 * via the runtime options.
 *
 * @note A block manager instance can be configured
 *       such that it is thread safe.
 */
struct ChiBlockManager_ {
    ChiBlockList freeList;
    size_t       blocksCount, blockSize, chunkSize;
    uintptr_t    blockMask;
    ChiWord*     nextBlock;
    ChiWord*     chunkEnd;
    ChiChunkList chunkList;
    ChiRuntime*  rt;
    ChiMutex     mutex;
    bool         shared, allocHookEnabled;
};

CHI_INTERN void chiBlockManagerSetup(ChiBlockManager*, size_t, size_t, bool, ChiRuntime*);
CHI_INTERN void chiBlockManagerDestroy(ChiBlockManager*);
CHI_INTERN CHI_WU CHI_RET_NONNULL ChiBlock* chiBlockManagerAlloc(ChiBlockManager*);
CHI_INTERN void chiBlockManagerFree(ChiBlockManager*, ChiBlock*);
CHI_INTERN void chiBlockManagerFreeList(ChiBlockManager*, ChiBlockList*);

// call outs
CHI_INTERN CHI_WU ChiChunk* chiBlockManagerChunkNew(ChiBlockManager*, size_t, size_t);
CHI_INTERN void chiBlockManagerChunkFree(ChiBlockManager*, ChiChunk*);
CHI_INTERN void chiBlockManagerAllocHook(ChiBlockManager*);

CHI_INL CHI_WU ChiBlock* chiBlock(ChiBlockManager* bm, void* p) {
    ChiBlock* b = (ChiBlock*)((uintptr_t)p & bm->blockMask);
    CHI_ASSERT_POISONED(b->canary, CHI_POISON_BLOCK_CANARY);
    CHI_ASSERT_POISONED(b->owner, bm);
    return b;
}
