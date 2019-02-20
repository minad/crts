#pragma once

#include "debug.h"

typedef struct ChiList_ ChiList;

/**
 * Circular double-linked list
 */
struct ChiList_ {
    ChiList *prev, *next;
};

#define CHI_LIST_INIT(h) ((ChiList){ .prev = &h, .next = &h })

#define _CHI_LIST_ENTRY(type, field, list, entry) ((entry) == (list) ? 0 : CHI_OUTER(type, field, entry))

// Supports deletions!
#define _CHI_LIST_FOREACH(nextEntry, type, field, entry, list)              \
    for (type* entry = _CHI_LIST_ENTRY(type, field, list, (list)->next), \
             * nextEntry   = entry ? _CHI_LIST_ENTRY(type, field, list, entry->field.next) : 0; \
         entry;                                                         \
         entry = nextEntry, nextEntry = entry ? _CHI_LIST_ENTRY(type, field, list, entry->field.next) : 0)
#define CHI_LIST_FOREACH(type, field, entry, list) _CHI_LIST_FOREACH(CHI_GENSYM, type, field, entry, list)

// Doesn't support deletion!
#define CHI_LIST_FOREACH_NODELETE(type, field, entry, list)           \
    for (type* entry = _CHI_LIST_ENTRY(type, field, list, (list)->next); \
         entry;                                                         \
         entry = _CHI_LIST_ENTRY(type, field, list, entry->field.next))

#if !defined(NDEBUG) && CHI_POISON_ENABLED
#  define CHI_LIST_POISONED(h)    CHI_ASSERT(chiListPoisoned(h))
#  define CHI_LIST_NONPOISONED(h) ({ CHI_ASSERT(h->next); CHI_ASSERT(h->prev); CHI_ASSERT(!chiListPoisoned(h)); })
#  define CHI_LIST_INIT_POISONED  ((ChiList){ .prev = (ChiList*)CHI_POISON_LIST, .next = (ChiList*)CHI_POISON_LIST })
#else
#  define CHI_LIST_POISONED(h)    ({})
#  define CHI_LIST_NONPOISONED(h) ({})
#  define CHI_LIST_INIT_POISONED  ((ChiList){ 0, 0 })
#endif

CHI_INL bool chiListPoisoned(const ChiList* e) {
    return CHI_POISONED(e->next, CHI_POISON_LIST) && CHI_POISONED(e->prev, CHI_POISON_LIST);
}

CHI_INL void chiListPoison(ChiList* e) {
    CHI_POISON(e->next, CHI_POISON_LIST);
    CHI_POISON(e->prev, CHI_POISON_LIST);
}

CHI_INL void chiListInit(ChiList* h) {
    h->next = h->prev = h;
}

CHI_INL bool chiListNull(const ChiList* h) {
    CHI_LIST_NONPOISONED(h);
    return h == h->next;
}

// insert after h
CHI_INL void chiListPrepend(ChiList* h, ChiList* e) {
    CHI_LIST_NONPOISONED(h);
    CHI_LIST_POISONED(e);
    e->prev = h;
    e->next = h->next;
    h->next = h->next->prev = e;
}

// insert before h
CHI_INL void chiListAppend(ChiList* h, ChiList* e) {
    CHI_LIST_NONPOISONED(h);
    CHI_LIST_POISONED(e);
    e->next = h;
    e->prev = h->prev;
    h->prev = h->prev->next = e;
}

CHI_INL void chiListDelete(ChiList* e) {
    CHI_LIST_NONPOISONED(e);
    e->next->prev = e->prev;
    e->prev->next = e->next;
    chiListPoison(e);
}

CHI_INL ChiList* chiListPop(ChiList* h) {
    CHI_LIST_NONPOISONED(h);
    ChiList* e = h->next;
    chiListDelete(e);
    return e;
}

// appends list b to the end of a
CHI_INL void chiListJoin(ChiList* a, ChiList* b) {
    CHI_LIST_NONPOISONED(a);
    CHI_LIST_NONPOISONED(b);
    CHI_ASSERT(a != b);
    if (!chiListNull(b)) {
        b->next->prev = a->prev;
        b->prev->next = a;
        a->prev->next = b->next;
        a->prev = b->prev;
        chiListInit(b);
    }
}
