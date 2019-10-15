// Generated by generate.pl from defs.in
CHI_INL CHI_WU bool chiEventFilterEnabled(const void* bits, uint32_t n) {
    return CHI_EVENT_ENABLED && CHI_UNLIKELY(_chiBitGet((const uintptr_t*)bits, n));
}

#if CHI_EVENT_ENABLED

CHI_EXPORT CHI_WU const void* chiEventFilter(void);
CHI_EXPORT void chiEvent_THREAD_SCHED_BEGIN(void);
CHI_EXPORT void chiEvent_THREAD_SCHED_END(uint32_t);
CHI_EXPORT void chiEvent_USER_DURATION_BEGIN(void);
CHI_EXPORT void chiEvent_USER_DURATION_END(void);
CHI_EXPORT void chiEvent_ENTRY_BLACKHOLE(void);
CHI_EXPORT void chiEvent_ENTRY_START(void);
CHI_EXPORT void chiEvent_ENTRY_TIMER_INT(void);
CHI_EXPORT void chiEvent_ENTRY_UNHANDLED(void);
CHI_EXPORT void chiEvent_ENTRY_USER_INT(void);
CHI_EXPORT void chiEvent_PROC_MSG_RECV(uint32_t);
CHI_EXPORT void chiEvent_PROC_MSG_SEND(Chili, uint32_t);
CHI_EXPORT void chiEvent_THREAD_ENQUEUE(uint32_t, uint32_t);
CHI_EXPORT void chiEvent_THREAD_MIGRATE(uint32_t, Chili);
CHI_EXPORT void chiEvent_THREAD_NAME(uint32_t, Chili);
CHI_EXPORT void chiEvent_THREAD_TAKEOVER(uint32_t, Chili);
CHI_EXPORT void chiEvent_THREAD_TERMINATED(void);
CHI_EXPORT void chiEvent_THREAD_YIELD(uint32_t);
CHI_EXPORT void chiEvent_USER_BUFFER(Chili);
CHI_EXPORT void chiEvent_USER_STRING(Chili);

#else

CHI_EXPORT_INL CHI_WU const void* chiEventFilter(void) { return 0; }
CHI_EXPORT_INL void chiEvent_THREAD_SCHED_BEGIN(void) {}
CHI_EXPORT_INL void chiEvent_THREAD_SCHED_END(uint32_t CHI_UNUSED(a0)) {}
CHI_EXPORT_INL void chiEvent_USER_DURATION_BEGIN(void) {}
CHI_EXPORT_INL void chiEvent_USER_DURATION_END(void) {}
CHI_EXPORT_INL void chiEvent_ENTRY_BLACKHOLE(void) {}
CHI_EXPORT_INL void chiEvent_ENTRY_START(void) {}
CHI_EXPORT_INL void chiEvent_ENTRY_TIMER_INT(void) {}
CHI_EXPORT_INL void chiEvent_ENTRY_UNHANDLED(void) {}
CHI_EXPORT_INL void chiEvent_ENTRY_USER_INT(void) {}
CHI_EXPORT_INL void chiEvent_PROC_MSG_RECV(uint32_t CHI_UNUSED(a0)) {}
CHI_EXPORT_INL void chiEvent_PROC_MSG_SEND(Chili CHI_UNUSED(a0), uint32_t CHI_UNUSED(a1)) {}
CHI_EXPORT_INL void chiEvent_THREAD_ENQUEUE(uint32_t CHI_UNUSED(a0), uint32_t CHI_UNUSED(a1)) {}
CHI_EXPORT_INL void chiEvent_THREAD_MIGRATE(uint32_t CHI_UNUSED(a0), Chili CHI_UNUSED(a1)) {}
CHI_EXPORT_INL void chiEvent_THREAD_NAME(uint32_t CHI_UNUSED(a0), Chili CHI_UNUSED(a1)) {}
CHI_EXPORT_INL void chiEvent_THREAD_TAKEOVER(uint32_t CHI_UNUSED(a0), Chili CHI_UNUSED(a1)) {}
CHI_EXPORT_INL void chiEvent_THREAD_TERMINATED(void) {}
CHI_EXPORT_INL void chiEvent_THREAD_YIELD(uint32_t CHI_UNUSED(a0)) {}
CHI_EXPORT_INL void chiEvent_USER_BUFFER(Chili CHI_UNUSED(a0)) {}
CHI_EXPORT_INL void chiEvent_USER_STRING(Chili CHI_UNUSED(a0)) {}

#endif