#pragma once

#include "private.h"

CHI_INL bool chiBitGet(const uint8_t* bits, size_t n) {
    return (uint8_t)(bits[n / 8] >> (n & 7)) & 1;
}

CHI_INL void chiBitSet(uint8_t* bits, size_t n, bool set) {
    uint8_t shift = n & 7, old = bits[n / 8];
    bits[n / 8] = (uint8_t)(((old & ~(1 << shift)) | (set << shift)));
}

CHI_INL bool chiAtomicBitGet(const _Atomic(uint8_t)* bits, size_t n) {
    return (uint8_t)(bits[n / 8] >> (n & 7)) & 1;
}

CHI_INL bool chiAtomicBitCas(_Atomic(uint8_t)* bits, size_t n, bool oldbit, bool newbit, memory_order order) {
    for (;;) {
        uint8_t shift = n & 7, old = bits[n / 8];
        if (((old >> shift) & 1) != oldbit)
            return false;
        uint8_t val = (uint8_t)((old & ~(1 << shift)) | (newbit << shift));
        // The order makes a difference on arm64.
        if (atomic_compare_exchange_weak_explicit((bits + n / 8), &old, val, order, memory_order_relaxed))
            return true;
    }
}
