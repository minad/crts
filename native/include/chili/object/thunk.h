/**
 * @file
 *
 * Thunk representation:
 *
 *     1. Thunk not forced꞉
 *
 *         ┌─────┬───┐   ┌───────┬───┐   ┌─────────┬──────┬──────┬────────┐
 *         │ ... │ *─┼──▶│ THUNK │ *─┼──▶│ THUNKFN │ Cont │ Var0 │ Var... │
 *         └─────┴───┘   └───────┴───┘   └─────────┴──────┴──────┴────────┘
 *
 *     2.a Thunk forced (Boxed)꞉
 *
 *         ┌─────┬───┐   ┌───────┬───┐   ┌────────┐
 *         │ ... │ *─┼──▶│ THUNK │ *─┼──▶│ Object │
 *         └─────┴───┘   └───────┴───┘   └────────┘
 *
 *     2.b Thunk forced (Unboxed)꞉
 *
 *         ┌─────┬───┐   ┌───────┬───┐
 *         │ ... │ *─┼──▶│ THUNK │ U │
 *         └─────┴───┘   └───────┴───┘
 *
 *     3.a Collapsed (Boxed)꞉
 *
 *         ┌─────┬───┐   ┌────────┐
 *         │ ... │ *─┼──▶│ Object │
 *         └─────┴───┘   └────────┘
 *
 *     3.b Collapsed (Unboxed)꞉
 *
 *         ┌─────┬───┐
 *         │ ... │ U │
 *         └─────┴───┘
 */
#pragma once

#include "../../chili.h"

CHI_OBJECT(THUNK, Thunk, ChiAtomic val)

CHI_INL CHI_WU bool _chiIsThunk(Chili thunk) {
    return !_chiUnboxed(thunk) && _chiType(thunk) == CHI_THUNK;
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiThunkForced)(Chili thunk, Chili* val) {
    if (!_chiIsThunk(thunk)) { // Collapsed thunk
        *val = thunk;
        return true;
    }
    *val = _chiAtomicLoad(&chiToThunk(thunk)->val);
    if (_chiUnboxed(*val) || _chiType(*val) != CHI_THUNKFN) // Evaluated thunk
        return true;
    return false;
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiThunkCollapsible)(Chili thunk, Chili* val) {
    return _chiThunkForced(thunk, val) && !_chiIsThunk(*val);
}

CHI_INL Chili chiThunkInit(Chili thunk, Chili clos) {
    CHI_ASSERT(_chiType(clos) == CHI_THUNKFN);
    _chiAtomicInit(&chiToThunk(thunk)->val, clos);
    return thunk;
}
