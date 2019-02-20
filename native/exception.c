#include "exception.h"
#include "adt.h"
#include "runtime.h"

CHI_COLD Chili chiAsyncException(ChiAsyncException e, Chili trace) {
    Chili show = chiNewFn(1, &chiShowAsyncException);
    Chili id = CHI_STATIC_STRING("AsyncException"); // TODO use uint128 identifier
    Chili identifier = chiNewTuple(id, id);
    Chili info = chiNewTuple(identifier, show);
    return chiNewTuple(info, chiFromUnboxed(e), trace);
}

CHI_COLD Chili chiRuntimeException(Chili str, Chili trace) {
    Chili show = chiNewFn(1, &chiIdentity);
    Chili id = CHI_STATIC_STRING("RuntimeException"); // TODO use uint128 identifier
    Chili identifier = chiNewTuple(id, id);
    Chili info = chiNewTuple(identifier, show);
    return chiNewTuple(info, str, trace);
}
