#include "../mem.h"
#include "test.h"

#define S_ELEM       int
#define S_LESS(a, b) (*(a) < *(b))
#include "../generic/sort.h"

TEST(sort) {
    for (size_t n = 0; n < 1000; ++n) {
        CHI_AUTO_ALLOC(int, a, n);

        for (size_t i = 0; i < n; ++i)
            a[i] = (int)RAND;
        _insert_sort_int(a, n);
        ASSERT(_sorted_int(a, n));
        _insert_sort_int(a, n);
        ASSERT(_sorted_int(a, n));

        for (size_t i = 0; i < n; ++i)
            a[i] = (int)RAND;
        sortint(a, n);
        ASSERT(_sorted_int(a, n));
        sortint(a, n);
        ASSERT(_sorted_int(a, n));
    }
}
