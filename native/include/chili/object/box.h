CHI_OBJECT(BOX, Box, ChiWord value)

CHI_EXPORT CHI_WU Chili CHI_PRIVATE(chiBoxNew)(ChiWord);

CHI_INL CHI_WU bool _chiInt64Unboxed(int64_t i) {
    return ((i << 1) >> 1) == i;
}

CHI_INL CHI_WU Chili CHI_PRIVATE(chiInt64Pack)(int64_t i) {
    CHI_ASSERT(_chiInt64Unboxed(i));
    return _chiFromUnboxed(((ChiWord)i << 1) >> 1);
}

CHI_INL CHI_WU int64_t CHI_PRIVATE(chiInt64Unpack)(Chili c) {
    CHI_ASSERT(_chiUnboxed63(c));
    return ((int64_t)_chiToUnboxed(c) << 1) >> 1;
}

CHI_INL CHI_WU Chili chiFromUInt64(uint64_t i) {
    return CHI_INT64_UNBOXING && CHI_LIKELY(_chiFitsUnboxed63(i)) ? _chiFromUnboxed(i) : _chiBoxNew(i);
}

CHI_INL CHI_WU uint64_t chiToUInt64(Chili c) {
    return CHI_INT64_UNBOXING && CHI_LIKELY(_chiUnboxed63(c)) ? _chiToUnboxed(c) : _chiToBox(c)->value;
}

CHI_INL CHI_WU Chili chiFromInt64(int64_t i) {
    return CHI_INT64_UNBOXING && CHI_LIKELY(_chiInt64Unboxed(i)) ? _chiInt64Pack(i) : _chiBoxNew((uint64_t)i);
}

CHI_INL CHI_WU int64_t chiToInt64(Chili c) {
    return CHI_INT64_UNBOXING && CHI_LIKELY(_chiUnboxed63(c)) ? _chiInt64Unpack(c) : (int64_t)_chiToBox(c)->value;
}

CHI_INL CHI_WU Chili chiFromFloat64(double f) {
    if (CHI_NANBOXING_ENABLED) {
        Chili c = { f == f ? _chiFloat64ToBits(f) : _CHILI_NAN };
        CHI_ASSERT(!_chiRef(c));
        return c;
    }
    return CHI_FLOAT63_ENABLED
        ? _chiFromUnboxed(_chiFloat64ToBits(f) >> 1)
        : _chiBoxNew(_chiFloat64ToBits(f));
}

CHI_INL CHI_WU double chiToFloat64(Chili c) {
    if (CHI_NANBOXING_ENABLED) {
        CHI_ASSERT(!_chiRef(c));
        return _chiBitsToFloat64(CHILI_UN(c));
    }
    return CHI_FLOAT63_ENABLED
        ? _chiBitsToFloat64(_chiToUnboxed(c) << 1)
        : _chiBitsToFloat64(_chiToBox(c)->value);
}
