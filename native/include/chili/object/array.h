CHI_EXPORT void chiArrayCopy(Chili, uint32_t, Chili, uint32_t, uint32_t);
CHI_EXPORT CHI_WU Chili chiArrayTryClone(Chili, uint32_t, uint32_t);
CHI_EXPORT CHI_WU Chili chiArrayNewFlags(uint32_t, Chili, ChiNewFlags);
CHI_EXPORT void chiArrayWrite(Chili, uint32_t, Chili);
CHI_EXPORT CHI_WU bool chiArrayCas(Chili, uint32_t, Chili, Chili);

CHI_INL CHI_WU uint32_t chiArraySize(Chili c) {
    CHI_ASSERT(_chiType(c) == CHI_ARRAY_SMALL || _chiType(c) == CHI_ARRAY_LARGE);
    return (uint32_t)_chiSize(c);
}

CHI_INL CHI_WU ChiField* CHI_PRIVATE(chiArrayField)(Chili c, uint32_t idx) {
    CHI_ASSERT(!_chiEmpty(c));
    CHI_ASSERT(idx < chiArraySize(c));
    return (ChiField*)_chiRawPayload(c) + idx;
}

CHI_INL void CHI_PRIVATE(chiArrayInit)(Chili c, uint32_t idx, Chili val) {
    _chiFieldInit(_chiArrayField(c, idx), val);
}

CHI_INL CHI_WU Chili chiArrayRead(Chili c, uint32_t idx) {
    return _chiFieldRead(_chiArrayField(c, idx));
}
