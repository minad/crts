#pragma once

#include "list.h"

#define CHI_FOREACH_BLOCK(block, blocklist)          CHI_LIST_FOREACH(ChiBlock, list, block, &(blocklist)->list)
#define CHI_FOREACH_BLOCK_NODELETE(block, blocklist) CHI_LIST_FOREACH_NODELETE(ChiBlock, list, block, &(blocklist)->list)

/**
 * Heap generation
 * We use an enum for a bit more type safety,
 * since the compiler warns if enums are cast to int.
 */
typedef enum {
    CHI_GEN_NURSERY = 0,
    CHI_GEN_MAJOR   = 0xFF, // must contain all minor bits
} ChiGen;

/**
 * Memory block used by ChiBlockManager and ChiBlockAlloc
 */
typedef struct CHI_WORD_ALIGNED {
    uint32_t  canary;
    ChiGen    gen;      ///< Generation of all objects in this block, used to lookup object generation
    ChiWord   *base;    ///< Base pointer
    ChiWord   *ptr;     ///< Current allocation pointer
    ChiWord   *end;     ///< Pointer to end of block
    ChiList   list;     ///< Linked list of blocks
    uint32_t  _pad[5 * (CHI_ARCH_BITS == 32)];
    uint8_t   marked[]; ///< Marking bits
} ChiBlock;

// 2 mark bits * CHI_BLOCK_MINSIZE / 64
_Static_assert(CHI_SIZEOF_WORDS(ChiBlock) + 2 * CHI_BLOCK_MINSIZE / 64 + CHI_BLOCK_MINLIMIT == CHI_BLOCK_MINSIZE, "Wrong block limit");

_Static_assert(offsetof(ChiBlock, marked) % CHI_WORDSIZE == 0, "Marking bits should be aligned");

/**
 * List of blocks which also keeps count
 * of blocks in the current list.
 */
typedef struct {
    ChiList list;
    size_t  count;
} ChiBlockList;

CHI_INL ChiBlock* chiBlock(void* p, uintptr_t mask) {
    ChiBlock* b = (ChiBlock*)((uintptr_t)p & mask);
    CHI_ASSERT_POISONED(b->canary, CHI_POISON_BLOCK_CANARY);
    return b;
}

CHI_INL bool chiBlockListNull(const ChiBlockList* list) {
    bool n = !list->count;
    CHI_ASSERT(chiListNull(&list->list) == n);
    return n;
}

CHI_INL void chiBlockListInit(ChiBlockList* list) {
    chiListInit(&list->list);
    list->count = 0;
}

CHI_INL void chiBlockListAppend(ChiBlockList* list, ChiBlock* b) {
    chiListAppend(&list->list, &b->list);
    ++list->count;
}

CHI_INL void chiBlockListPrepend(ChiBlockList* list, ChiBlock* b) {
    chiListPrepend(&list->list, &b->list);
    ++list->count;
}

CHI_INL void chiBlockListDelete(ChiBlockList* list, ChiBlock* b) {
    chiListDelete(&b->list);
    --list->count;
}

CHI_INL CHI_WU ChiBlock* chiOuterBlock(ChiList* entry) {
    return CHI_OUTER(ChiBlock, list, entry);
}

CHI_INL ChiBlock* chiBlockListHead(const ChiBlockList* list) {
    return chiBlockListNull(list) ? 0 : chiOuterBlock(list->list.next);
}

CHI_INL CHI_WU ChiBlock* chiBlockListPop(ChiBlockList* list) {
    CHI_ASSERT(!chiBlockListNull(list));
    --list->count;
    return chiOuterBlock(chiListPop(&list->list));
}

CHI_INL void chiBlockListJoin(ChiBlockList* dst, ChiBlockList* src) {
    CHI_ASSERT(dst != src);
    chiListJoin(&dst->list, &src->list);
    dst->count += src->count;
    src->count = 0;
}

CHI_INL size_t chiBlockIndex(ChiBlock* b, void* p) {
    //CHI_ASSERT((ChiWord*)p >= b->base); base is invalidated by scavenging!
    CHI_ASSERT((uint8_t*)p > b->marked);
    CHI_ASSERT((ChiWord*)p < b->end);
    CHI_ASSERT((uintptr_t)p % CHI_WORDSIZE == 0);
    return (size_t)((ChiWord*)p - (const ChiWord*)b);
}
