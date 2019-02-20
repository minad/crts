#pragma once

#include "../../chili.h"

CHI_OBJECT(THREAD, Thread, ChiAtomic name, state, local, stack; Chili exceptBlock, lastErrno, tid)

CHI_CONT_DECL(extern CHI_API, chiThreadYield)
CHI_CONT_DECL(extern CHI_API, chiProcessorStart)
CHI_CONT_DECL(extern CHI_API, chiProcessorSuspend)
CHI_CONT_DECL(extern CHI_API, chiProcessorStop)

CHI_API void chiThreadSetLocal(Chili, Chili);
CHI_API void chiThreadSetName(Chili, Chili);
CHI_API void chiThreadSetState(Chili, Chili);
CHI_API CHI_WU Chili chiThreadCurrent(void);
CHI_API CHI_WU Chili chiThreadNew(Chili);
CHI_API CHI_WU uint32_t chiProcessorId(void);
CHI_API void chiProcessorResume(uint32_t);

CHI_API_INL void chiThreadSetExceptionBlock(Chili thread, uint32_t n) {
    chiToThread(thread)->exceptBlock = chiFromUInt32(n);
}

CHI_API_INL CHI_WU uint32_t chiThreadExceptionBlock(Chili thread) {
    return chiToUInt32(chiToThread(thread)->exceptBlock);
}

CHI_API_INL void chiThreadSetErrno(Chili thread, int32_t n) {
    chiToThread(thread)->lastErrno = chiFromInt32(n);
}

CHI_API_INL CHI_WU int32_t chiThreadErrno(Chili thread) {
    return chiToInt32(chiToThread(thread)->lastErrno);
}

CHI_API_INL CHI_WU Chili chiThreadLocal(Chili thread) {
    return _chiAtomicLoad(&chiToThread(thread)->local);
}

CHI_API_INL CHI_WU Chili chiThreadName(Chili thread) {
    return _chiAtomicLoad(&chiToThread(thread)->name);
}

CHI_API_INL CHI_WU Chili chiThreadState(Chili thread) {
    return _chiAtomicLoad(&chiToThread(thread)->state);
}
