/**
 * @file
 *
 * Thunk representation:
 *
 *     1. Thunk unforced:
 *
 *         ┌─────┬───┐   ┌───────┬──────┬──────┬────────┐
 *         │ ... │ *─┼──▶│ THUNK │ Cont │ self │ Var... │
 *         └─────┴───┘   └───────┴──────┴──────┴────────┘
 *
 *     2. Thunk blackholed:
 *
 *         ┌─────┬───┐   ┌───────┬───────────────────┬──────┬────────┐
 *         │ ... │ *─┼──▶│ THUNK │ chiThunkBlackhole │ self │ Var... │
 *         └─────┴───┘   └───────┴───────────────────┴──────┴────────┘
 *
 *     3.a Thunk forced (Boxed):
 *
 *         ┌─────┬───┐   ┌───────┬───────────────────┬───┬────────┐
 *         │ ... │ *─┼──▶│ THUNK │ chiThunkBlackhole │ * │ Var... │
 *         └─────┴───┘   └───────┴───────────────────┴─┼─┴────────┘
 *                                                     ▼
 *                                                 ┌────────┐
 *                                                 │ Object │
 *                                                 └────────┘
 *
 *     3.b Thunk forced (Unboxed):
 *
 *         ┌─────┬───┐   ┌───────┬───────────────────┬───┬────────┐
 *         │ ... │ *─┼──▶│ THUNK │ chiThunkBlackhole │ U │ Var... │
 *         └─────┴───┘   └───────┴───────────────────┴───┴────────┘
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

CHI_OBJECT(THUNK, Thunk, ChiField cont, val; Chili clos[])

CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiThunkBlackhole))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiThunkUpdateCont))

CHI_INL CHI_WU bool CHI_PRIVATE(chiThunkForced)(Chili thunk, Chili* val) {
    *val = _chiFieldRead(&_chiToThunk(thunk)->val);
    return !chiIdentical(*val, thunk);
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiThunkCollapsible)(ChiThunk* thunk, Chili* collapsed) {
    // Thunks which reference other thunks are not collapsible.
    // - Unforced thunks are referencing themselves
    // - Forwarded thunks reference the forwarded thunk
    *collapsed = _chiFieldRead(&thunk->val);
    return !_chiRefType(*collapsed, CHI_THUNK);
}
