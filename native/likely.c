// Inspired by CCAN likely
#include "hashfn.h"
#include "sink.h"

#if CHI_LIKELY_STATS_ENABLED

typedef struct {
    const char* key;
    _Atomic(uint64_t) right, wrong;
} Record;

static Record likelyRecord[4096] = {};

CHI_DESTRUCTOR(likely) {
    for (size_t i = 0; i < CHI_DIM(likelyRecord); ++i) {
        const Record* r = likelyRecord + i;
        uint64_t total = r->right + r->wrong;
        if (total > 100) {
            uint64_t ratio = 100 * r->right / total;
            if (ratio < 99) {
                chiSinkFmt(chiStderr, "%s ratio=%ju%% right=%ju wrong=%ju\n",
                              r->key, ratio, r->right, r->wrong);
            }
        }
    }
}

CHI_INL bool likely(bool val, bool expect, const char* key) {
    Record* r = likelyRecord + CHI_UN(Hash, chiHashPtr(key)) % CHI_DIM(likelyRecord);
    if (!r->key)
        r->key = key;
    if (r->key == key)
        atomic_fetch_add_explicit(val == expect ? &r->right : &r->wrong, 1, memory_order_relaxed);
    return val;
}

bool chiLikely(bool val, const char* key) {
    return likely(val, true, key);
}

bool chiUnlikely(bool val, const char* key) {
    return likely(val, false, key);
}

#endif
