/**
 * @file
 *
 * Thunk representation:
 *
 *     1. Thunk unforced:
 *
 *         ┌─────┬───┐   ┌───────┬───┐   ┌──────────┬──────┬────────┐
 *         │ ... │ *─┼──▶│ THUNK │ *─┼──▶│ THUNK_FN │ Cont │ Var... │
 *         └─────┴───┘   └───────┴───┘   └──────────┴──────┴────────┘
 *
 *     2. Thunk blackholed:
 *
 *         ┌─────┬───┐   ┌───────┬───┐   ┌──────────┬─────────────────┬────────┐
 *         │ ... │ *─┼──▶│ THUNK │ *─┼──▶│ THUNK_FN │chiThunkBlackhole│ Var... │
 *         └─────┴───┘   └───────┴───┘   └──────────┴─────────────────┴────────┘
 *
 *     3.a Thunk forced (Boxed):
 *
 *         ┌─────┬───┐   ┌───────┬───┐   ┌────────┐
 *         │ ... │ *─┼──▶│ THUNK │ *─┼──▶│ Object │
 *         └─────┴───┘   └───────┴───┘   └────────┘
 *
 *     3.b Thunk forced (Unboxed):
 *
 *         ┌─────┬───┐   ┌───────┬───┐
 *         │ ... │ *─┼──▶│ THUNK │ U │
 *         └─────┴───┘   └───────┴───┘
 *
 *     4.a Collapsed (Boxed):
 *
 *         ┌─────┬───┐   ┌────────┐
 *         │ ... │ *─┼──▶│ Object │
 *         └─────┴───┘   └────────┘
 *
 *     4.b Collapsed (Unboxed):
 *
 *         ┌─────┬───┐
 *         │ ... │ U │
 *         └─────┴───┘
 */

CHI_OBJECT(THUNK, Thunk, ChiField val)

CHI_INL CHI_WU bool CHI_PRIVATE(chiThunkForced)(Chili thunk, Chili* val) {
    if (_chiUnboxed(thunk) || _chiType(thunk) != CHI_THUNK) { // Collapsed thunk
        *val = thunk;
        return true;
    }
    *val = _chiFieldRead(&_chiToThunk(thunk)->val);
    return _chiUnboxed(*val) || _chiType(*val) != CHI_THUNK_FN; // Evaluated thunk
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiThunkCollapsible)(Chili val) {
    if (_chiUnboxed(val))
        return true;
    ChiType t = _chiType(val);
    return t != CHI_THUNK_FN && t != CHI_THUNK;
}
