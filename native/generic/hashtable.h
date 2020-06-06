#include "../hashfn.h"

#define HT_CREATE        CHI_CAT(HT_PREFIX, Create)
#define HT_DELETE        CHI_CAT(HT_PREFIX, Delete)
#define HT_FIND          CHI_CAT(HT_PREFIX, Find)
#define HT_FINDPOS       CHI_CAT(HT_PREFIX, FindPos)
#define HT_FN            CHI_CAT(HT_PREFIX, Fn)
#define HT_FREE          CHI_CAT(HT_PREFIX, Free)
#define HT_GROW          CHI_CAT(HT_PREFIX, Grow)
#define HT_INSERT        CHI_CAT(HT_PREFIX, Insert)
#define HT_INSERTPOS     CHI_CAT(HT_PREFIX, InsertPos)

#ifndef HT_EXISTS
#  define HT_EXISTS HT_KEY
#endif
#ifndef HT_KEYTYPE
#  define HT_KEYTYPE const typeof (HT_KEY(((HT_ENTRY*)0)))
#endif
#ifndef HT_KEYEQ
#  define HT_KEYEQ(a, b) (a == b)
#endif
#ifndef HT_HASHFN
#  define HT_HASHFN(k) chiHashPtr(k)
#endif

#define HT_INVARIANT                                    \
    ({                                                  \
        CHI_ASSERT(h);                                  \
        CHI_ASSERT((!h->entry) == (!h->capacity));      \
        CHI_ASSERT(h->used <= h->capacity);             \
    })
#define HT_FULL      (h->used >= CHI_HT_MAXFILL * h->capacity / 100)

#ifndef CHI_HT_INIT
#  define CHI_HT_INIT { 0, 0, 0 }
#  define CHI_HT_AUTO(h, n) h n __attribute__ ((cleanup(CHI_CAT(h, _auto_free)))) = CHI_HT_INIT
#  define CHI_HT_FOREACH(t, e, h) for (typeof(CHI_CAT(t, _next)((h), (h)->entry)) e = CHI_CAT(t, _next)((h), (h)->entry); e; e = CHI_CAT(t, _next)((h), e + 1))
#endif

#ifndef HT_NOSTRUCT
typedef struct {
    HT_ENTRY* entry;
    size_t used, capacity;
} HT_HASH;
#endif

/**
 * Compute hash index
 */
CHI_INL CHI_WU ChiHash HT_FN(HT_KEYTYPE k) {
    return HT_HASHFN(k);
}

/**
 * Free hash table
 */
CHI_INL void HT_FREE(HT_HASH* h) {
    HT_INVARIANT;
    chiFree(h->entry);
    h->entry = 0;
    h->capacity = h->used = 0;
}

CHI_INL void CHI_CAT(HT_HASH, _auto_free)(HT_HASH* h) {
    HT_FREE(h);
}

/**
 * Get next entry in the hash table
 *
 * @param h Hash table
 * @param e Current entry
 * @return Next entry
 */
CHI_INL CHI_WU HT_ENTRY* CHI_CAT(HT_HASH, _next)(const HT_HASH* h, HT_ENTRY* e) {
    HT_INVARIANT;
    for (; e < h->entry + h->capacity; ++e) {
        bool x = HT_EXISTS(e);
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
CHI_INL CHI_WU HT_ENTRY* HT_INSERTPOS(HT_HASH* h, ChiHash i) {
    HT_INVARIANT;
    CHI_ASSERT(h && h->entry);
    for (size_t n = 0; ; ++n) {
        HT_ENTRY* e = h->entry + ((CHI_UN(Hash, i) + n) & (h->capacity - 1));
        bool x = HT_EXISTS(e);
        if (!x)
            return e;
    }
}

/**
 * Grow hash table by factor of two
 */
CHI_LOCAL void HT_GROW(HT_HASH* h) {
    HT_INVARIANT;
    HT_HASH l = *h;
    l.capacity = h->capacity ? 2 * h->capacity : (1UL << CHI_HT_INITSHIFT);
    l.entry = chiZallocArr(HT_ENTRY, l.capacity);
    CHI_HT_FOREACH(HT_HASH, e, h)
        *HT_INSERTPOS(&l, HT_FN(HT_KEY(e))) = *e;
    HT_FREE(h);
    *h = l;
}

/**
 * Insert into hash table
 *
 * @param h Hash table
 * @param i Hash index
 * @return Pointer to inserted entry
 */
CHI_INL CHI_WU HT_ENTRY* HT_INSERT(HT_HASH* h, ChiHash i) {
    HT_INVARIANT;
    if (CHI_UNLIKELY(HT_FULL))
        HT_GROW(h);
    ++h->used;
    return HT_INSERTPOS(h, i);
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
CHI_INL CHI_WU bool HT_FINDPOS(const HT_HASH* h, HT_KEYTYPE k, ChiHash i, HT_ENTRY** r) {
    HT_INVARIANT;
    CHI_ASSERT(h && h->entry);
    for (size_t n = 0; ; ++n) {
        HT_ENTRY* e = h->entry + ((CHI_UN(Hash, i) + n) & (h->capacity - 1));
        bool x = HT_EXISTS(e);
        if (!x) {
            *r = e;
            return false;
        }
        HT_KEYTYPE l = HT_KEY(e);
        bool q = HT_KEYEQ(l, k);
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
CHI_INL CHI_WU HT_ENTRY* HT_FIND(const HT_HASH* h, HT_KEYTYPE k) {
    HT_ENTRY* e;
    return h->entry && HT_FINDPOS(h, k, HT_FN(k), &e) ? e : 0;
}

/**
 * Delete entry by key
 *
 * @param h Hash table
 * @param k Key
 * @return true if found
 */
CHI_INL CHI_WU bool HT_DELETE(HT_HASH* h, HT_KEYTYPE k) {
    if (!h->entry)
        return false;
    ChiHash i = HT_FN(k);
    HT_ENTRY* e;
    if (!HT_FINDPOS(h, k, i, &e))
        return false;
    HT_ENTRY* f = e;
    for (size_t shift = 1;;) {
        f = h->entry + (((size_t)(f - h->entry) + 1) & (h->capacity - 1));
        bool x = HT_EXISTS(f);
        if (!x)
            break;
        HT_KEYTYPE l = HT_KEY(f);
        if ((((size_t)(f - h->entry) - CHI_UN(Hash, HT_FN(l))) & (h->capacity - 1)) >= shift) {
            *e = *f;
            e = f;
            shift = 1;
        } else {
            ++shift;
        }
    }
    --h->used;
    CHI_ZERO_STRUCT(e);
    return true;
}

/**
 * Create new entry or find if it already exists
 *
 * @param h Hash table
 * @param k Key
 * @param r Pointer to entry
 * @return true if the entry was newly created
 */
CHI_INL CHI_WU bool HT_CREATE(HT_HASH* h, HT_KEYTYPE k, HT_ENTRY** r) {
    HT_INVARIANT;
    ChiHash i = HT_FN(k);
    if (CHI_LIKELY(h->entry)) {
        HT_ENTRY* e;
        if (HT_FINDPOS(h, k, i, &e)) {
            *r = e;
            return false;
        }
        if (CHI_LIKELY(!HT_FULL)) {
            ++h->used;
            *r = e;
            return true;
        }
    }
    *r = HT_INSERT(h, i);
    return true;
}

#undef HT_CREATE
#undef HT_DELETE
#undef HT_ENTRY
#undef HT_EXISTS
#undef HT_FIND
#undef HT_FINDPOS
#undef HT_FN
#undef HT_FREE
#undef HT_FULL
#undef HT_GROW
#undef HT_HASH
#undef HT_HASHFN
#undef HT_INSERT
#undef HT_INSERT
#undef HT_INVARIANT
#undef HT_KEY
#undef HT_KEYEQ
#undef HT_KEYTYPE
#undef HT_NOSTRUCT
#undef HT_PREFIX
