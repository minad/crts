#pragma once

#include "../../chili.h"

CHI_INL CHI_WU ChiAtomic* CHI_PRIVATE(chiArrayField)(Chili array, uint32_t idx) {
    CHI_ASSERT(_chiType(array) == CHI_ARRAY);
    CHI_ASSERT(idx < chiSize(array));
    return (ChiAtomic*)_chiRawPayload(array) + idx;
}

CHI_API void chiArrayCopy(Chili, uint32_t, Chili, uint32_t, uint32_t);
CHI_API void chiArrayWrite(Chili, uint32_t, Chili);
CHI_API CHI_WU Chili chiArrayTryClone(Chili, uint32_t, uint32_t);
CHI_API CHI_WU Chili chiArrayTryNew(uint32_t, Chili);
CHI_API CHI_WU Chili chiArrayNewFlags(uint32_t, Chili, uint32_t);
CHI_API CHI_WU bool chiArrayCas(Chili, uint32_t, Chili, Chili);

CHI_API_INL CHI_WU Chili chiArrayRead(Chili array, uint32_t idx) {
    return _chiAtomicLoad(_chiArrayField(array, idx));
}
