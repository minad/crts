#include "exception.h"
#include "new.h"
#include "runtime.h"
#include "stack.h"

Chili chiAsyncException(ChiProcessor* proc, Chili* sp, ChiAsyncException e) {
    Chili show = chiNewFn(proc, 1, &chiShowAsyncException);
    Chili id = chiStringNew(proc, "AsyncException"); // TODO use uint128 identifier
    Chili identifier = chiNewTuple(proc, id, id);
    Chili info = chiNewTuple(proc, identifier, show);
    return chiNewTuple(proc, info, chiFromUnboxed(e), chiStackGetTrace(proc, sp));
}

Chili chiRuntimeException(ChiProcessor* proc, Chili* sp, Chili msg) {
    Chili show = chiNewFn(proc, 1, &chiIdentity);
    Chili id = chiStringNew(proc, "RuntimeException"); // TODO use uint128 identifier
    Chili identifier = chiNewTuple(proc, id, id);
    Chili info = chiNewTuple(proc, identifier, show);
    return chiNewTuple(proc, info, msg, chiStackGetTrace(proc, sp));
}
