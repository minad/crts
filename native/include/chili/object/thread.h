CHI_OBJECT(THREAD, Thread, ChiField state, stack)

CHI_EXPORT_CONT_DECL(chiThreadSwitch)
CHI_EXPORT_CONT_DECL(chiProcessorPark)
CHI_EXPORT_CONT_DECL(chiProcessorInitLocal)

CHI_EXPORT void chiThreadInitState(Chili);
CHI_EXPORT CHI_WU Chili chiThreadNew(Chili, Chili);
CHI_EXPORT void chiProcessorSuspend(Chili, uint32_t);
CHI_EXPORT CHI_WU uint32_t chiProcessorCount(void);
CHI_EXPORT CHI_WU bool chiProcessorTryStart(Chili);
CHI_EXPORT void chiProcessorStop(void);

CHI_EXPORT_INL CHI_WU Chili chiThreadState(Chili thread) {
    return _chiFieldRead(&_chiToThread(thread)->state);
}
