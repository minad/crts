CHI_OBJECT(ARRAY, Array, ChiField field[1])

CHI_EXPORT void chiArrayCopy(Chili, uint32_t, Chili, uint32_t, uint32_t);
CHI_EXPORT CHI_WU Chili chiArrayTryClone(Chili, uint32_t, uint32_t);
CHI_EXPORT CHI_WU Chili chiArrayNewFlags(uint32_t, Chili, ChiNewFlags);
CHI_EXPORT void chiArrayWrite(Chili, uint32_t, Chili);
CHI_EXPORT CHI_WU bool chiArrayCas(Chili, uint32_t, Chili, Chili);

CHI_INL CHI_WU ChiField* CHI_PRIVATE(chiArrayField)(Chili c, uint32_t idx) {
    CHI_ASSERT(!_chiEmpty(c));
    CHI_ASSERT(idx < _chiSize(c));
    return _chiToArray(c)->field + idx;
}

CHI_INL void CHI_PRIVATE(chiArrayInit)(Chili c, uint32_t idx, Chili val) {
    CHI_ASSERT(!_chiEmpty(c));
    CHI_ASSERT(idx < _chiSize(c));
    _chiFieldInit(_chiToArray(c)->field + idx, val);
}

CHI_INL CHI_WU Chili chiArrayRead(Chili c, uint32_t idx) {
    return _chiFieldRead(_chiArrayField(c, idx));
}
