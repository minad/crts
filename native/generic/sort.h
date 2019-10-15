#ifndef S_SUFFIX
#  define S_SUFFIX S_ELEM
#endif
#define S_INSERT_SORT  CHI_CAT(_insert_sort_, S_SUFFIX)
#define S_SORTED       CHI_CAT(_sorted_, S_SUFFIX)
#define S_LESSFN       CHI_CAT(_less_, S_SUFFIX)
#define S_PARTITION    CHI_CAT(_partition, S_SUFFIX)
#define S_SORT         CHI_CAT(sort, S_SUFFIX)

CHI_INL bool S_LESSFN(const S_ELEM* a, const S_ELEM* b) {
    return S_LESS(a, b);
}

static CHI_NOINL void S_INSERT_SORT(S_ELEM* a, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        size_t j = i;
        S_ELEM t = a[j];
        for (; j > 0 && S_LESSFN(&t, a + j - 1); --j)
            a[j] = a[j - 1];
        a[j] = t;
    }
}

CHI_INL bool S_SORTED(S_ELEM* a, size_t n) {
    for (size_t i = 0; n > 1 && i < n - 1; ++i) {
        if (S_LESSFN(a + i + 1, a + i))
            return false;
    }
    return true;
}

CHI_INL size_t S_PARTITION(S_ELEM* a, size_t n) {
    S_ELEM *p = a - 1, *q = a + n;
    for (;;) {
        do { ++p; } while (S_LESSFN(p, a));
        do { --q; } while (S_LESSFN(a, q));
        if (p >= q)
            return (size_t)(q - a);
        CHI_SWAP(*p, *q);
    }
}

static void S_SORT(S_ELEM* a, size_t n) {
    CHI_ASSERT(a || !n);

    if (S_SORTED(a, n))
        return;

    if (n < CHI_QUICK_SORT_CUTOFF) {
        S_INSERT_SORT(a, n);
        return;
    }

    size_t p = S_PARTITION(a, n);
    S_SORT(a, p + 1);
    S_SORT(a + p + 1, n - 1 - p);
}

#undef S_INSERT_SORT
#undef S_LESS
#undef S_LESSFN
#undef S_PARTITION
#undef S_SORT
#undef S_SORTED
#undef S_ELEM
#undef S_SUFFIX
