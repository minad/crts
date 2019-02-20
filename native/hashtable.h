#include "hashfn.h"

#define CREATE        CHI_CAT(PREFIX, Create)
#define DESTROY       CHI_CAT(PREFIX, Destroy)
#define FIND          CHI_CAT(PREFIX, Find)
#define FINDPOS       CHI_CAT(PREFIX, FindPos)
#define GROW          CHI_CAT(PREFIX, Grow)
#define INDEX         CHI_CAT(PREFIX, Index)
#define INSERT        CHI_CAT(PREFIX, Insert)
#define INSERTPOS     CHI_CAT(PREFIX, InsertPos)
#define KEYTYPE       CHI_CAT(PREFIX, KeyType)
#define NEXT          CHI_CAT(PREFIX, Next)

#ifndef ZALLOCARR
#  define ZALLOCARR(h, n) chiZallocArr(ENTRY, n)
#  define FREE(h, p)   chiFree(p)
#endif
#ifndef MAXFILL
#  define MAXFILL CHI_HASH_MAXFILL
#endif
#ifndef INITSHIFT
#  define INITSHIFT CHI_HASH_INITSHIFT
#endif
#ifndef EXISTS
#  define EXISTS KEY
#endif
#ifndef KEYEQ
#  define KEYEQ(a, b) (a == b)
#endif
#ifndef HASHFN
#  define HASHFN(k) chiHashPtr(k)
#endif

#define INVARIANT                                       \
    ({                                                  \
        CHI_ASSERT(h);                                  \
        CHI_ASSERT((!h->entry) == (!h->capacity));    \
        CHI_ASSERT(h->used <= h->capacity);           \
    })
#define FULL      (h->used >= MAXFILL * h->capacity / 100)
#define HASH_INIT { 0, 0, 0 }
#define HASH_FOREACH(p, e, h) for (typeof(CHI_CAT(p, Next)((h), (h)->entry)) e = CHI_CAT(p, Next)((h), (h)->entry); e; e = CHI_CAT(p, Next)((h), e + 1))
#define HASH_AUTO(h, n) h n __attribute__ ((cleanup(CHI_CAT(h, _auto_destroy)))) = HASH_INIT

#ifndef NOHASH
typedef struct {
    ENTRY* entry;
    size_t used, capacity;
} HASH;
#endif

typedef const typeof (KEY(((ENTRY*)0))) KEYTYPE;

/**
 * Compute hash index
 */
CHI_INL CHI_WU ChiHashIndex INDEX(KEYTYPE k) {
    return HASHFN(k);
}

/**
 * Destroy hash table and free allocated memory
 */
CHI_INL void DESTROY(HASH* h) {
    INVARIANT;
    FREE(h, h->entry);
    h->entry = 0;
    h->capacity = h->used = 0;
}

CHI_INL void CHI_CAT(HASH, _auto_destroy)(HASH* h) {
    DESTROY(h);
}

/**
 * Get next entry in the hash table
 *
 * @param h Hash table
 * @param e Current entry
 * @return Next entry
 */
CHI_INL CHI_WU ENTRY* NEXT(const HASH* h, ENTRY* e) {
    INVARIANT;
    for (; e < h->entry + h->capacity; ++e) {
        bool x = EXISTS(e);
        if (x)
            return e;
    }
    return 0;
}

/**
 * Get next free insertion position in the hash table
 *
 * @param h Hash table
 * @param i Hash index
 * @return Pointer to free entry
 */
CHI_INL CHI_WU ENTRY* INSERTPOS(HASH* h, ChiHashIndex i) {
    INVARIANT;
    CHI_ASSERT(h && h->entry);
    for (size_t n = 0; ; ++n) {
        size_t a = (CHI_UN(HashIndex, i) + n) & (h->capacity - 1);
        ENTRY* e = h->entry + a;
        bool x = EXISTS(e);
        if (!x)
            return e;
    }
}

/**
 * Grow hash table by factor of two
 */
static void GROW(HASH* h) {
    INVARIANT;
    HASH l = *h;
    l.capacity = h->capacity ? 2 * h->capacity : (1UL << INITSHIFT);
    l.entry = ZALLOCARR(h, l.capacity);
    HASH_FOREACH(PREFIX, e, h)
        memcpy(INSERTPOS(&l, INDEX(KEY(e))), e, sizeof (ENTRY));
    DESTROY(h);
    *h = l;
}

/**
 * Insert into hash table
 *
 * @param h Hash table
 * @param i Hash index
 * @return Pointer to inserted entry
 */
CHI_INL CHI_WU ENTRY* INSERT(HASH* h, ChiHashIndex i) {
    INVARIANT;
    if (CHI_UNLIKELY(FULL))
        GROW(h);
    ++h->used;
    return INSERTPOS(h, i);
}

/**
 * Find position of entry or insertion position
 *
 * @param h Hash table
 * @param k Key
 * @param i Hash index of key
 * @param r Returns pointer to entry or next insertion position
 * @return true if key was found
 */
CHI_INL CHI_WU bool FINDPOS(const HASH* h, KEYTYPE k, ChiHashIndex i, ENTRY** r) {
    INVARIANT;
    CHI_ASSERT(h && h->entry);
    for (size_t n = 0; ; ++n) {
        size_t a = (CHI_UN(HashIndex, i) + n) & (h->capacity - 1);
        ENTRY* e = h->entry + a;
        bool x = EXISTS(e);
        if (!x) {
            *r = e;
            return false;
        }
        KEYTYPE l = KEY(e);
        bool q = KEYEQ(l, k);
        if (q) {
            *r = e;
            return true;
        }
    }
}

/**
 * Find entry by key
 *
 * @param h Hash table
 * @param k Key
 * @return Entry or 0 if not found
 */
CHI_INL CHI_WU ENTRY* FIND(const HASH* h, KEYTYPE k) {
    ENTRY* e;
    return h->entry && FINDPOS(h, k, INDEX(k), &e) ? e : 0;
}

/**
 * Create new entry or find if it already exists
 *
 * @param h Hash table
 * @param k Key
 * @param r Pointer to entry
 * @return true if the entry was newly created
 */
CHI_INL CHI_WU bool CREATE(HASH* h, KEYTYPE k, ENTRY** r) {
    INVARIANT;
    ChiHashIndex i = INDEX(k);
    if (CHI_LIKELY(h->entry)) {
        ENTRY* e;
        if (FINDPOS(h, k, i, &e)) {
            *r = e;
            return false;
        }
        if (CHI_LIKELY(!FULL)) {
            ++h->used;
            *r = e;
            return true;
        }
    }
    *r = INSERT(h, i);
    return true;
}

#undef ZALLOCARR
#undef CREATE
#undef DESTROY
#undef ENTRY
#undef EXISTS
#undef FIND
#undef FINDPOS
#undef FREE
#undef FULL
#undef GROW
#undef HASH
#undef HASHFN
#undef INDEX
#undef INITSHIFT
#undef INSERT
#undef INSERT
#undef INVARIANT
#undef KEY
#undef KEYEQ
#undef KEYTYPE
#undef MAXFILL
#undef NEXT
#undef NOHASH
#undef PREFIX
