#include "../debug.h"

#ifdef LIST_LENGTH
#  undef LIST_LENGTH
#  define LIST_LENGTH(...)    __VA_ARGS__
#  define LIST_NOLENGTH(...)
#else
#  define LIST_LENGTH(...)
#  define LIST_NOLENGTH(...) __VA_ARGS__
#endif

#define LIST_APPEND CHI_CAT(LIST_PREFIX, Append)
#define LIST_DELETE CHI_CAT(LIST_PREFIX, Delete)
#define LIST_HEAD CHI_CAT(LIST_PREFIX, Head)
#define LIST_INIT CHI_CAT(LIST_PREFIX, Init)
#define LIST_INSERT_AFTER CHI_CAT(LIST_PREFIX, InsertAfter)
#define LIST_INSERT_BEFORE CHI_CAT(LIST_PREFIX, InsertBefore)
#define LIST_JOIN CHI_CAT(LIST_PREFIX, Join)
#define LIST_LINK_AFTER CHI_CAT(LIST_PREFIX, LinkAfter)
#define LIST_LINK_BEFORE CHI_CAT(LIST_PREFIX, LinkBefore)
#define LIST_NEXT CHI_CAT(LIST_PREFIX, Next)
#define LIST_NULL CHI_CAT(LIST_PREFIX, Null)
#define LIST_POISON CHI_CAT(LIST_PREFIX, Poison)
#define LIST_POP CHI_CAT(LIST_PREFIX, Pop)
#define LIST_PREPEND CHI_CAT(LIST_PREFIX, Prepend)
#define LIST_PREV CHI_CAT(LIST_PREFIX, Prev)

#ifndef LIST
#  define LIST CHI_CAT(LIST_ELEM, List)
#endif

#ifndef LIST_LINK
#  define LIST_LINK CHI_CAT(LIST_ELEM, Link)
#endif

#ifndef LIST_LINK_FIELD
#  define LIST_LINK_FIELD link
#endif

#ifndef LIST_NOSTRUCT
typedef struct {
    LIST_LINK head;
    LIST_LENGTH(size_t length;)
} LIST;
#endif

#ifndef CHI_LIST_INIT
#define CHI_LIST_INIT(l) { .head = { .prev = &(l).head, .next = &(l).head }, }

#define _CHI_LIST_ENTRY(type, field, list, entry) ((entry) == &(list)->head ? 0 : CHI_OUTER(type, field, entry))

// Supports deletions!
#define _CHI_LIST_FOREACH(nextEntry, type, field, entry, list)              \
    for (type* entry = _CHI_LIST_ENTRY(type, field, list, (list)->head.next), \
             * nextEntry   = entry ? _CHI_LIST_ENTRY(type, field, list, entry->field.next) : 0; \
         entry;                                                         \
         entry = nextEntry, nextEntry = entry ? _CHI_LIST_ENTRY(type, field, list, entry->field.next) : 0)
#define CHI_LIST_FOREACH(type, field, entry, list) _CHI_LIST_FOREACH(CHI_GENSYM, type, field, entry, list)

// Doesn't support deletion!
#define CHI_LIST_FOREACH_NODELETE(type, field, entry, list)           \
    for (type* entry = _CHI_LIST_ENTRY(type, field, list, (list)->head.next); \
         entry;                                                         \
         entry = _CHI_LIST_ENTRY(type, field, list, entry->field.next))

#if CHI_POISON_ENABLED
#  define _CHI_LIST_POISONED(k)   (CHI_POISONED((k)->next, CHI_POISON_LIST) && CHI_POISONED((k)->prev, CHI_POISON_LIST))
#  define CHI_LIST_POISONED(k)    CHI_ASSERT(_CHI_LIST_POISONED(k))
#  define CHI_LIST_NONPOISONED(k) ({ CHI_ASSERT((k)->next); CHI_ASSERT((k)->prev); CHI_ASSERT(!_CHI_LIST_POISONED(k)); })
#else
#  define CHI_LIST_POISONED(k) ({})
#  define CHI_LIST_NONPOISONED(k)   ({})
#endif
#endif

CHI_INL void LIST_POISON(LIST_ELEM* e) {
    LIST_LINK* k = &e->LIST_LINK_FIELD;
    CHI_NOWARN_UNUSED(k);
    CHI_POISON(k->next, CHI_POISON_LIST);
    CHI_POISON(k->prev, CHI_POISON_LIST);
}

CHI_INL void LIST_INIT(LIST* l) {
    l->head.next = l->head.prev = &l->head;
    LIST_LENGTH(l->length = 0;)
}

CHI_INL bool LIST_NULL(const LIST* l) {
    CHI_LIST_NONPOISONED(&l->head);
    LIST_NOLENGTH(return &l->head == l->head.next;)
    LIST_LENGTH(CHI_ASSERT((l->length == 0) == (&l->head == l->head.next)); return !l->length;)
}

CHI_INL LIST_ELEM* LIST_HEAD(const LIST* l) {
    return LIST_NULL(l) ? 0 : CHI_OUTER(LIST_ELEM, LIST_LINK_FIELD, l->head.next);
}

CHI_INL LIST_ELEM* LIST_PREV(const LIST* l, const LIST_ELEM* e) {
    const LIST_LINK* k = &e->LIST_LINK_FIELD;
    CHI_LIST_NONPOISONED(&l->head);
    CHI_LIST_NONPOISONED(k);
    return k->prev == &l->head ? 0 : CHI_OUTER(LIST_ELEM, LIST_LINK_FIELD, k->prev);
}

CHI_INL LIST_ELEM* LIST_NEXT(const LIST* l, const LIST_ELEM* e) {
    const LIST_LINK* k = &e->LIST_LINK_FIELD;
    CHI_LIST_NONPOISONED(&l->head);
    CHI_LIST_NONPOISONED(k);
    return k->next == &l->head ? 0 : CHI_OUTER(LIST_ELEM, LIST_LINK_FIELD, k->next);
}

CHI_INL void LIST_LINK_AFTER(LIST_LENGTH(LIST* l,) LIST_LINK* a, LIST_LINK* b) {
    CHI_LIST_NONPOISONED(a);
    CHI_LIST_POISONED(b);
    b->prev = a;
    b->next = a->next;
    a->next = a->next->prev = b;
    LIST_LENGTH(++l->length;)
}

CHI_INL void LIST_LINK_BEFORE(LIST_LENGTH(LIST* l,) LIST_LINK* a, LIST_LINK* b) {
    CHI_LIST_NONPOISONED(a);
    CHI_LIST_POISONED(b);
    b->next = a;
    b->prev = a->prev;
    a->prev = a->prev->next = b;
    LIST_LENGTH(++l->length;)
}

CHI_INL void LIST_INSERT_AFTER(LIST_LENGTH(LIST* l,) LIST_ELEM* a, LIST_ELEM* b) {
    LIST_LINK_AFTER(LIST_LENGTH(l,) &a->LIST_LINK_FIELD, &b->LIST_LINK_FIELD);
}

CHI_INL void LIST_INSERT_BEFORE(LIST_LENGTH(LIST* l,) LIST_ELEM* a, LIST_ELEM* b) {
    LIST_LINK_BEFORE(LIST_LENGTH(l,) &a->LIST_LINK_FIELD, &b->LIST_LINK_FIELD);
}

CHI_INL void LIST_PREPEND(LIST* l, LIST_ELEM* e) {
    LIST_LINK_AFTER(LIST_LENGTH(l,) &l->head, &e->LIST_LINK_FIELD);
}

CHI_INL void LIST_APPEND(LIST* l, LIST_ELEM* e) {
    LIST_LINK_BEFORE(LIST_LENGTH(l,) &l->head, &e->LIST_LINK_FIELD);
}

CHI_INL void LIST_DELETE(LIST_LENGTH(LIST* l,) LIST_ELEM* e) {
    LIST_LINK* k = &e->LIST_LINK_FIELD;
    CHI_LIST_NONPOISONED(k);
    k->next->prev = k->prev;
    k->prev->next = k->next;
    LIST_LENGTH(CHI_ASSERT(l->length > 0); --l->length;)
    LIST_POISON(e);
}

CHI_INL LIST_ELEM* LIST_POP(LIST* l) {
    CHI_ASSERT(!LIST_NULL(l));
    LIST_ELEM* e = CHI_OUTER(LIST_ELEM, LIST_LINK_FIELD, l->head.next);
    LIST_DELETE(LIST_LENGTH(l,) e);
    return e;
}

// appends list b to the end of a
CHI_INL void LIST_JOIN(LIST* a, LIST* b) {
    CHI_LIST_NONPOISONED(&a->head);
    CHI_LIST_NONPOISONED(&b->head);
    CHI_ASSERT(a != b);
    if (!LIST_NULL(b)) {
        b->head.next->prev = a->head.prev;
        b->head.prev->next = &a->head;
        a->head.prev->next = b->head.next;
        a->head.prev = b->head.prev;
        LIST_LENGTH(a->length += b->length;)
        LIST_INIT(b);
    }
}

#undef LIST
#undef LIST_APPEND
#undef LIST_DELETE
#undef LIST_ELEM
#undef LIST_HEAD
#undef LIST_INIT
#undef LIST_INSERT_AFTER
#undef LIST_INSERT_BEFORE
#undef LIST_JOIN
#undef LIST_LINK
#undef LIST_LINK_AFTER
#undef LIST_LINK_BEFORE
#undef LIST_LINK_FIELD
#undef LIST_NEXT
#undef LIST_NOSTRUCT
#undef LIST_NULL
#undef LIST_POISON
#undef LIST_POP
#undef LIST_PREFIX
#undef LIST_PREV
#undef LIST_LENGTH
#undef LIST_NOLENGTH
